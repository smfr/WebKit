/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteScrollingTreeMac.h"

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#import "RemoteScrollingCoordinatorProxy.h"
#import "ScrollingTreeFrameScrollingNodeRemoteMac.h"
#import "ScrollingTreeOverflowScrollingNodeRemoteMac.h"
#import <WebCore/ScrollingTreeFixedNodeCocoa.h>
#import <WebCore/WebCoreCALayerExtras.h>

namespace WebKit {
using namespace WebCore;

Ref<RemoteScrollingTree> RemoteScrollingTree::create(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
{
    return adoptRef(*new RemoteScrollingTreeMac(scrollingCoordinator));
}

RemoteScrollingTreeMac::RemoteScrollingTreeMac(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : RemoteScrollingTree(scrollingCoordinator)
{
}

RemoteScrollingTreeMac::~RemoteScrollingTreeMac() = default;

void RemoteScrollingTreeMac::handleWheelEventPhase(ScrollingNodeID, PlatformWheelEventPhase)
{
    // FIXME: Is this needed?
}

void RemoteScrollingTreeMac::hasNodeWithAnimatedScrollChanged(bool hasNodeWithAnimatedScroll)
{
    m_scrollingCoordinatorProxy.hasNodeWithAnimatedScrollChanged(hasNodeWithAnimatedScroll);
}

void RemoteScrollingTreeMac::displayDidRefresh(PlatformDisplayID)
{
    Locker locker { m_treeLock };
    serviceScrollAnimations(MonotonicTime::now()); // FIXME: Share timestamp with the rest of the update.
}

Ref<ScrollingTreeNode> RemoteScrollingTreeMac::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeRemoteMac::create(*this, nodeType, nodeID);

    case ScrollingNodeType::Overflow:
        return ScrollingTreeOverflowScrollingNodeRemoteMac::create(*this, nodeID);

    case ScrollingNodeType::FrameHosting:
    case ScrollingNodeType::OverflowProxy:
    case ScrollingNodeType::Fixed:
    case ScrollingNodeType::Sticky:
    case ScrollingNodeType::Positioned:
        return RemoteScrollingTree::createScrollingTreeNode(nodeType, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
}

void RemoteScrollingTreeMac::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
    if (!rootNode())
        return;
    static_cast<ScrollingTreeFrameScrollingNodeRemoteMac&>(*rootNode()).handleMouseEvent(event);
}

using LayerAndPoint = std::pair<CALayer *, FloatPoint>;

static void collectDescendantLayersAtPoint(Vector<LayerAndPoint, 16>& layersAtPoint, CALayer *parent, CGPoint point)
{
    if (parent.masksToBounds && ![parent containsPoint:point])
        return;

    if (parent.mask && ![parent _web_maskContainsPoint:point])
        return;

    for (CALayer *layer in [parent sublayers]) {
        CALayer *layerWithResolvedAnimations = layer;

        if ([[layer animationKeys] count])
            layerWithResolvedAnimations = [layer presentationLayer];

        auto transform = TransformationMatrix { [layerWithResolvedAnimations transform] };
        if (!transform.isInvertible())
            continue;

        CGPoint subviewPoint = [layerWithResolvedAnimations convertPoint:point fromLayer:parent];

        auto handlesEvent = [&] {
            if (CGRectIsEmpty([layerWithResolvedAnimations frame]))
                return false;

            if (![layerWithResolvedAnimations containsPoint:subviewPoint])
                return false;

            auto* layerTreeNode = RemoteLayerTreeNode::forCALayer(layer);
            if (layerTreeNode) {
                // Scrolling changes boundsOrigin on the scroll container layer, but we computed its event region ignoring scroll position, so factor out bounds origin.
                FloatPoint boundsOrigin = layer.bounds.origin;
                FloatPoint localPoint = subviewPoint - toFloatSize(boundsOrigin);
                auto& eventRegion = layerTreeNode->eventRegion();

                bool result = eventRegion.contains(roundedIntPoint(localPoint));
                LOG_WITH_STREAM(UIHitTesting, stream << " RemoteLayerTreeNode " << layerTreeNode->layerID() << " eventRegion " << eventRegion << " contains " << result);
                return result;
            }

            return false;
        }();

        if (handlesEvent)
            layersAtPoint.append(std::make_pair(layer, subviewPoint));

        if ([layer sublayers])
            collectDescendantLayersAtPoint(layersAtPoint, layer, subviewPoint);
    };
}

static ScrollingNodeID scrollingNodeIDForLayer(CALayer *layer)
{
    auto* layerTreeNode = RemoteLayerTreeNode::forCALayer(layer);
    if (!layerTreeNode)
        return 0;

    return layerTreeNode->scrollingNodeID();
}

static bool isScrolledBy(const ScrollingTree& tree, ScrollingNodeID scrollingNodeID, CALayer *hitLayer)
{
    for (CALayer *layer = hitLayer; layer; layer = [layer superlayer]) {
        auto nodeID = scrollingNodeIDForLayer(layer);
        if (nodeID == scrollingNodeID)
            return true;

        auto* scrollingNode = tree.nodeForID(nodeID);
        if (is<ScrollingTreeOverflowScrollProxyNode>(scrollingNode)) {
            ScrollingNodeID actingOverflowScrollingNodeID = downcast<ScrollingTreeOverflowScrollProxyNode>(*scrollingNode).overflowScrollingNodeID();
            if (actingOverflowScrollingNodeID == scrollingNodeID)
                return true;
        }

        if (is<ScrollingTreePositionedNode>(scrollingNode)) {
            if (downcast<ScrollingTreePositionedNode>(*scrollingNode).relatedOverflowScrollingNodes().contains(scrollingNodeID))
                return false;
        }
    }

    return false;
}

RefPtr<ScrollingTreeNode> RemoteScrollingTreeMac::scrollingNodeForPoint(FloatPoint point)
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

//    Locker locker { m_layerHitTestMutex };

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeMac*>(rootScrollingNode)->rootContentsLayer();
    FloatPoint scrollOrigin = rootScrollingNode->scrollOrigin();
    auto pointInContentsLayer = point;
    pointInContentsLayer.moveBy(scrollOrigin);

    Vector<LayerAndPoint, 16> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, rootContentsLayer.get(), pointInContentsLayer);

    LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " found " << layersAtPoint.size() << " layers");
#if !LOG_DISABLED
    for (auto [layer, point] : WTF::makeReversedRange(layersAtPoint))
        LOG_WITH_STREAM(UIHitTesting, stream << " layer " << [layer description] << " scrolling node " << scrollingNodeIDForLayer(layer));
#endif

    if (layersAtPoint.size()) {
        auto* frontmostLayer = layersAtPoint.last().first;
        for (auto [layer, point] : WTF::makeReversedRange(layersAtPoint)) {
            auto nodeID = scrollingNodeIDForLayer(layer);

            auto* scrollingNode = nodeForID(nodeID);
            if (!is<ScrollingTreeScrollingNode>(scrollingNode))
                continue;

            if (isScrolledBy(*this, nodeID, frontmostLayer))
                return scrollingNode;

            // FIXME: Hit-test scroll indicator layers.
        }
    }

    LOG_WITH_STREAM(UIHitTesting, stream << "RemoteScrollingTreeMac " << this << " scrollingNodeForPoint " << point << " found no scrollable layers; using root node");
    return rootScrollingNode;
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
