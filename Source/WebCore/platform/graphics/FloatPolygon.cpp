/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FloatPolygon.h"

#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>

namespace WebCore {

namespace FloatPolygonInternal {
static inline float determinant(const FloatSize& a, const FloatSize& b)
{
    return a.width() * b.height() - a.height() * b.width();
}
}

static inline bool areCollinearPoints(const FloatPoint& p0, const FloatPoint& p1, const FloatPoint& p2)
{
    return !FloatPolygonInternal::determinant(p1 - p0, p2 - p0);
}

static inline bool areCoincidentPoints(const FloatPoint& p0, const FloatPoint& p1)
{
    return p0.x() == p1.x() && p0.y() == p1.y();
}

static inline unsigned nextVertexIndex(unsigned vertexIndex, unsigned nVertices, bool clockwise)
{
    return ((clockwise) ? vertexIndex + 1 : vertexIndex - 1 + nVertices) % nVertices;
}

static unsigned findNextEdgeVertexIndex(const FloatPolygon& polygon, unsigned vertexIndex1, bool clockwise)
{
    unsigned nVertices = polygon.numberOfVertices();
    unsigned vertexIndex2 = nextVertexIndex(vertexIndex1, nVertices, clockwise);

    while (vertexIndex2 && areCoincidentPoints(polygon.vertexAt(vertexIndex1), polygon.vertexAt(vertexIndex2)))
        vertexIndex2 = nextVertexIndex(vertexIndex2, nVertices, clockwise);

    while (vertexIndex2) {
        unsigned vertexIndex3 = nextVertexIndex(vertexIndex2, nVertices, clockwise);
        if (!areCollinearPoints(polygon.vertexAt(vertexIndex1), polygon.vertexAt(vertexIndex2), polygon.vertexAt(vertexIndex3)))
            break;
        vertexIndex2 = vertexIndex3;
    }

    return vertexIndex2;
}

FloatPolygon::FloatPolygon(Vector<FloatPoint>&& vertices)
    : m_vertices(WTFMove(vertices))
    , m_empty(m_vertices.size() < 3)
    , m_edges(m_vertices.size())
{
    unsigned nVertices = numberOfVertices();
    if (nVertices)
        m_boundingBox.setLocation(vertexAt(0));

    if (m_empty)
        return;

    unsigned minVertexIndex = 0;
    for (unsigned i = 1; i < nVertices; ++i) {
        const FloatPoint& vertex = vertexAt(i);
        if (vertex.y() < vertexAt(minVertexIndex).y() || (vertex.y() == vertexAt(minVertexIndex).y() && vertex.x() < vertexAt(minVertexIndex).x()))
            minVertexIndex = i;
    }
    FloatPoint nextVertex = vertexAt((minVertexIndex + 1) % nVertices);
    FloatPoint prevVertex = vertexAt((minVertexIndex + nVertices - 1) % nVertices);
    bool clockwise = FloatPolygonInternal::determinant(vertexAt(minVertexIndex) - prevVertex, nextVertex - prevVertex) > 0;

    unsigned edgeIndex = 0;
    unsigned vertexIndex1 = 0;
    do {
        m_boundingBox.extend(vertexAt(vertexIndex1));
        unsigned vertexIndex2 = findNextEdgeVertexIndex(*this, vertexIndex1, clockwise);
        m_edges[edgeIndex].m_polygon = this;
        m_edges[edgeIndex].m_vertexIndex1 = vertexIndex1;
        m_edges[edgeIndex].m_vertexIndex2 = vertexIndex2;
        m_edges[edgeIndex].m_edgeIndex = edgeIndex;
        ++edgeIndex;
        vertexIndex1 = vertexIndex2;
    } while (vertexIndex1);

    if (edgeIndex > 3) {
        const FloatPolygonEdge& firstEdge = m_edges[0];
        const FloatPolygonEdge& lastEdge = m_edges[edgeIndex - 1];
        if (areCollinearPoints(lastEdge.vertex1(), lastEdge.vertex2(), firstEdge.vertex2())) {
            m_edges[0].m_vertexIndex1 = lastEdge.m_vertexIndex1;
            edgeIndex--;
        }
    }

    m_edges.shrink(edgeIndex);
    m_empty = m_edges.size() < 3;

    if (m_empty)
        return;

    for (auto& edge : m_edges)
        m_edgeTree.add({ edge.minY(), edge.maxY(), &edge });
}

Vector<std::reference_wrapper<const FloatPolygonEdge>> FloatPolygon::overlappingEdges(float minY, float maxY) const
{
    auto overlappingEdgeIntervals = m_edgeTree.allOverlaps({ minY, maxY });
    return overlappingEdgeIntervals.map([](auto& interval) -> std::reference_wrapper<const FloatPolygonEdge> {
        return *interval.data();
    });
}

bool VertexPair::intersection(const VertexPair& other, FloatPoint& point) const
{
    // See: http://paulbourke.net/geometry/pointlineplane/, "Intersection point of two lines in 2 dimensions"

    const FloatSize& thisDelta = vertex2() - vertex1();
    const FloatSize& otherDelta = other.vertex2() - other.vertex1();
    float denominator = FloatPolygonInternal::determinant(thisDelta, otherDelta);
    if (!denominator)
        return false;

    // The two line segments: "this" vertex1,vertex2 and "other" vertex1,vertex2, have been defined
    // in parametric form. Each point on the line segment is: vertex1 + u * (vertex2 - vertex1),
    // when 0 <= u <= 1. We're computing the values of u for each line at their intersection point.

    const FloatSize& vertex1Delta = vertex1() - other.vertex1();
    float uThisLine = FloatPolygonInternal::determinant(otherDelta, vertex1Delta) / denominator;
    float uOtherLine = FloatPolygonInternal::determinant(thisDelta, vertex1Delta) / denominator;

    if (uThisLine < 0 || uOtherLine < 0 || uThisLine > 1 || uOtherLine > 1)
        return false;

    point = vertex1() + uThisLine * thisDelta;
    return true;
}

#ifndef NDEBUG

TextStream& operator<<(TextStream& stream, const FloatPolygonEdge& edge)
{
    return stream << &edge << " (" << edge.vertex1().x() << ',' << edge.vertex1().y() << ' ' << edge.vertex2().x() << ',' << edge.vertex2().y() << ')';
}

#endif

} // namespace WebCore
