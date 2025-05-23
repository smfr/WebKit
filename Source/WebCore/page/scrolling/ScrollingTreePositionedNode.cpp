/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScrollingTreePositionedNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "Logging.h"
#include "ScrollingStatePositionedNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeOverflowScrollingNode.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollingTreePositionedNode);

ScrollingTreePositionedNode::ScrollingTreePositionedNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNodeType::Positioned, nodeID)
{
}

ScrollingTreePositionedNode::~ScrollingTreePositionedNode() = default;

bool ScrollingTreePositionedNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    auto* positionedStateNode = dynamicDowncast<ScrollingStatePositionedNode>(stateNode);
    if (!positionedStateNode)
        return false;

    if (positionedStateNode->hasChangedProperty(ScrollingStateNode::Property::RelatedOverflowScrollingNodes))
        m_relatedOverflowScrollingNodes = positionedStateNode->relatedOverflowScrollingNodes();

    if (positionedStateNode->hasChangedProperty(ScrollingStateNode::Property::LayoutConstraintData))
        m_constraints = positionedStateNode->layoutConstraints();

    if (!m_relatedOverflowScrollingNodes.isEmpty())
        scrollingTree()->activePositionedNodes().add(*this);

    return true;
}

FloatSize ScrollingTreePositionedNode::scrollDeltaSinceLastCommit() const
{
    FloatSize delta;
    for (auto nodeID : m_relatedOverflowScrollingNodes) {
        if (auto* node = dynamicDowncast<ScrollingTreeOverflowScrollingNode>(scrollingTree()->nodeForID(nodeID)))
            delta += node->scrollDeltaSinceLastCommit();
    }

    // Positioned nodes compensate for scrolling, so negate the scroll delta.
    return -delta;
}

void ScrollingTreePositionedNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "positioned node"_s;
    ScrollingTreeNode::dumpProperties(ts, behavior);

    ts.dumpProperty("layout constraints"_s, m_constraints);
    ts.dumpProperty("related overflow nodes"_s, m_relatedOverflowScrollingNodes.size());

    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs) {
        if (!m_relatedOverflowScrollingNodes.isEmpty()) {
            TextStream::GroupScope scope(ts);
            ts << "overflow nodes"_s;
            for (auto nodeID : m_relatedOverflowScrollingNodes)
                ts << '\n' << indent << "nodeID "_s << nodeID;
        }
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
