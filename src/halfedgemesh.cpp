/*
 *  Copyright (c) 2020 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */
#include <unordered_map>
#include "halfedgemesh.h"

HalfedgeMesh::~HalfedgeMesh()
{
    // TODO:
}

HalfedgeMesh::HalfedgeMesh(const std::vector<Vector3> &vertices,
    const std::vector<std::vector<size_t>> &faces)
{
    std::vector<Vertex *> halfedgeVertices;
    halfedgeVertices.reserve(vertices.size());
    for (const auto &it: vertices) {
        Vertex *vertex = newVertex();
        vertex->position = it;
        halfedgeVertices.push_back(vertex);
    }
    
    std::vector<Face *> halfedgeFaces;
    halfedgeFaces.reserve(faces.size());
    for (const auto &it: faces) {
        Face *face = newFace();
        face->debugIndex = ++m_debugFaceIndex;
        halfedgeFaces.push_back(face);
    }

    std::unordered_map<uint64_t, Halfedge *> halfedgeMap;
    for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        const auto &indices = faces[faceIndex];
        if (3 != indices.size()) {
            std::cerr << "Found non-triangle, face count:" << indices.size() << std::endl;
            continue;
        }
        std::vector<Halfedge *> halfedges(indices.size());
        for (size_t i = 0; i < 3; ++i) {
            size_t j = (i + 1) % 3;
            const auto &first = indices[i];
            const auto &second = indices[j];
            
            Vertex *vertex = halfedgeVertices[first];
            Halfedge *halfedge = newHalfedge();
            halfedge->startVertex = vertex;
            halfedge->leftFace = halfedgeFaces[faceIndex];
            
            if (nullptr == halfedge->leftFace->halfedge) {
                halfedge->leftFace->halfedge = halfedge;
            }
            if (nullptr == vertex->firstHalfedge) {
                vertex->firstHalfedge = halfedge;
            }
            
            halfedges[i] = halfedge;
            
            auto insertResult = halfedgeMap.insert({makeHalfedgeKey(first, second), halfedge});
            if (!insertResult.second) {
                std::cerr << "Found repeated halfedge:" << first << "," << second << std::endl;
            }
        }
        linkFaceHalfedges(halfedges);
    }
    for (auto &it: halfedgeMap) {
        auto halfedgeIt = halfedgeMap.find(swapHalfedgeKey(it.first));
        if (halfedgeIt == halfedgeMap.end())
            continue;
        it.second->oppositeHalfedge = halfedgeIt->second;
        halfedgeIt->second->oppositeHalfedge = it.second;
    }
}

void HalfedgeMesh::linkFaceHalfedges(std::vector<HalfedgeMesh::Halfedge *> &halfedges)
{
    for (size_t i = 0; i < halfedges.size(); ++i) {
        size_t j = (i + 1) % halfedges.size();
        halfedges[i]->nextHalfedge = halfedges[j];
        halfedges[j]->previousHalfedge = halfedges[i];
    }
}

void HalfedgeMesh::updateFaceHalfedgesLeftFace(std::vector<HalfedgeMesh::Halfedge *> &halfedges,
    HalfedgeMesh::Face *leftFace)
{
    for (auto &it: halfedges)
        it->leftFace = leftFace;
}

void HalfedgeMesh::linkHalfedgePair(HalfedgeMesh::Halfedge *first, HalfedgeMesh::Halfedge *second)
{
    if (nullptr != first)
        first->oppositeHalfedge = second;
    if (nullptr != second)
        second->oppositeHalfedge = first;
}

double HalfedgeMesh::averageEdgeLength()
{
    double totalLength = 0.0;
    size_t halfedgeCount = 0;
    for (Face *face = m_firstFace; nullptr != face; face = face->nextFace) {
        const auto &startHalfedge = face->halfedge;
        Halfedge *halfedge = startHalfedge;
        do {
            const auto &nextHalfedge = halfedge->nextHalfedge;
            totalLength += (halfedge->startVertex->position - nextHalfedge->startVertex->position).length();
            ++halfedgeCount;
        } while (halfedge != startHalfedge);
    }
    if (0 == halfedgeCount)
        return 0.0;
    return totalLength / halfedgeCount;
}

HalfedgeMesh::Face *HalfedgeMesh::newFace()
{
    Face *face = new Face;
    face->debugIndex = ++m_debugFaceIndex;
    if (nullptr != m_lastFace) {
        m_lastFace->nextFace = face;
        face->previousFace = m_lastFace;
    } else {
        m_firstFace = face;
    }
    m_lastFace = face;
    return face;
}

HalfedgeMesh::Vertex *HalfedgeMesh::newVertex()
{
    Vertex *vertex = new Vertex;
    vertex->debugIndex = ++m_debugVertexIndex;
    if (nullptr != m_lastVertex) {
        m_lastVertex->nextVertex = vertex;
        vertex->previousVertex = m_lastVertex;
    } else {
        m_firstVertex = vertex;
    }
    m_lastVertex = vertex;
    return vertex;
}

HalfedgeMesh::Halfedge *HalfedgeMesh::newHalfedge()
{
    Halfedge *halfedge = new Halfedge;
    halfedge->debugIndex = ++m_debugHalfedgeIndex;
    return halfedge;
}

HalfedgeMesh::Face *HalfedgeMesh::moveToNextFace(HalfedgeMesh::Face *face)
{
    if (nullptr == face) {
        face = m_firstFace;
        if (nullptr == face)
            return nullptr;
        if (!face->removed)
            return face;
    }
    do {
        face = face->nextFace;
    } while (nullptr != face && face->removed);
    return face;
}

HalfedgeMesh::Vertex *HalfedgeMesh::moveToNextVertex(HalfedgeMesh::Vertex *vertex)
{
    if (nullptr == vertex) {
        vertex = m_firstVertex;
        if (nullptr == vertex)
            return nullptr;
        if (!vertex->removed)
            return vertex;
    }
    do {
        vertex = vertex->nextVertex;
    } while (nullptr != vertex && vertex->removed);
    return vertex;
}

void HalfedgeMesh::breakFace(HalfedgeMesh::Face *leftOldFace,
    HalfedgeMesh::Halfedge *halfedge,
    HalfedgeMesh::Vertex *breakPointVertex,
    std::vector<HalfedgeMesh::Halfedge *> &leftNewFaceHalfedges,
    std::vector<HalfedgeMesh::Halfedge *> &leftOldFaceHalfedges)
{
    std::vector<Halfedge *> leftFaceHalfedges = {
        halfedge->previousHalfedge,
        halfedge,
        halfedge->nextHalfedge
    };
    
    Face *leftNewFace = newFace();
    leftNewFace->halfedge = leftFaceHalfedges[2];
    leftFaceHalfedges[2]->leftFace = leftNewFace;
    
    leftNewFaceHalfedges.push_back(newHalfedge());
    leftNewFaceHalfedges.push_back(newHalfedge());
    leftNewFaceHalfedges.push_back(leftFaceHalfedges[2]);
    linkFaceHalfedges(leftNewFaceHalfedges);
    updateFaceHalfedgesLeftFace(leftNewFaceHalfedges, leftNewFace);
    leftNewFaceHalfedges[0]->startVertex = leftFaceHalfedges[0]->startVertex;
    leftNewFaceHalfedges[1]->startVertex = breakPointVertex;
    
    leftOldFaceHalfedges.push_back(leftFaceHalfedges[0]);
    leftOldFaceHalfedges.push_back(halfedge);
    leftOldFaceHalfedges.push_back(newHalfedge());
    linkFaceHalfedges(leftOldFaceHalfedges);
    updateFaceHalfedgesLeftFace(leftOldFaceHalfedges, leftOldFace);
    leftOldFaceHalfedges[2]->startVertex = breakPointVertex;
    
    breakPointVertex->firstHalfedge = leftNewFaceHalfedges[1];
    
    linkHalfedgePair(leftNewFaceHalfedges[0], leftOldFaceHalfedges[2]);
    
    leftOldFace->halfedge = leftOldFaceHalfedges[0];
}

void HalfedgeMesh::breakEdge(HalfedgeMesh::Halfedge *halfedge)
{
    Face *leftOldFace = halfedge->leftFace;
    Halfedge *oppositeHalfedge = halfedge->oppositeHalfedge;
    Face *rightOldFace = nullptr;
    
    if (nullptr != oppositeHalfedge)
        rightOldFace = oppositeHalfedge->leftFace;
    
    Vertex *breakPointVertex = newVertex();
    breakPointVertex->position = (halfedge->startVertex->position +
        halfedge->nextHalfedge->startVertex->position) * 0.5;
        
    //std::cout << "Break first half:" << leftOldFace->debugIndex << " halfedge:" << (int)halfedge << std::endl;
    
    std::vector<Halfedge *> leftNewFaceHalfedges;
    std::vector<Halfedge *> leftOldFaceHalfedges;
    breakFace(leftOldFace, halfedge, breakPointVertex,
        leftNewFaceHalfedges, leftOldFaceHalfedges);
    
    if (nullptr != rightOldFace) {
        //std::cout << "Break second half:" << rightOldFace->debugIndex << " halfedge:" << (int)oppositeHalfedge << std::endl;
        
        std::vector<Halfedge *> rightNewFaceHalfedges;
        std::vector<Halfedge *> rightOldFaceHalfedges;
        breakFace(rightOldFace, oppositeHalfedge, breakPointVertex,
            rightNewFaceHalfedges, rightOldFaceHalfedges);
        linkHalfedgePair(leftOldFaceHalfedges[1], rightNewFaceHalfedges[1]);
        linkHalfedgePair(leftNewFaceHalfedges[1], rightOldFaceHalfedges[1]);
    }
}

void HalfedgeMesh::changeVertexStartHalfedgeFrom(Vertex *vertex, Halfedge *halfedge)
{
    vertex->firstHalfedge = halfedge->previousHalfedge->oppositeHalfedge;
    if (nullptr != vertex->firstHalfedge)
        return;
    
    if (nullptr == halfedge->oppositeHalfedge)
        return;
    
    vertex->firstHalfedge = halfedge->oppositeHalfedge->nextHalfedge;
    return;
}

bool HalfedgeMesh::testLengthSquaredAroundVertex(Vertex *vertex, 
    const Vector3 &target, 
    double maxEdgeLengthSquared)
{
    const auto &startHalfedge = vertex->firstHalfedge;
    Halfedge *loopHalfedge = startHalfedge;
    if (nullptr != loopHalfedge->oppositeHalfedge) {
        do {
            if ((loopHalfedge->nextHalfedge->startVertex->position - 
                    target).lengthSquared() > maxEdgeLengthSquared) {
                return true;            
            }
            loopHalfedge = loopHalfedge->oppositeHalfedge;
            if (nullptr == loopHalfedge)
                break;
            loopHalfedge = loopHalfedge->nextHalfedge;
        } while (loopHalfedge != startHalfedge);
    } else {
        do {
            if ((loopHalfedge->nextHalfedge->startVertex->position - 
                    target).lengthSquared() > maxEdgeLengthSquared) {
                return true;            
            }
            loopHalfedge = loopHalfedge->previousHalfedge;
            if (nullptr == loopHalfedge)
                break;
            loopHalfedge = loopHalfedge->oppositeHalfedge;
        } while (loopHalfedge != startHalfedge);
    }
    return false;
}

void HalfedgeMesh::pointerVertexToNewVertex(Vertex *vertex, Vertex *replacement)
{
    const auto &startHalfedge = vertex->firstHalfedge;
    Halfedge *loopHalfedge = startHalfedge;
    size_t count = 0;
    if (nullptr != loopHalfedge->oppositeHalfedge) {
        do {
            //std::cout << "Update halfedge:" << loopHalfedge->debugIndex << " startVertex from " << loopHalfedge->startVertex->debugIndex << " to " << replacement->debugIndex << std::endl;
            loopHalfedge->startVertex = replacement;
            loopHalfedge = loopHalfedge->oppositeHalfedge;
            if (nullptr == loopHalfedge)
                break;
            loopHalfedge = loopHalfedge->nextHalfedge;
            if (count++ > 20) {
                abort();
            }
        } while (loopHalfedge != startHalfedge);
    } else {
        do {
            //std::cout << "Update halfedge:" << loopHalfedge->debugIndex << " startVertex from " << loopHalfedge->startVertex->debugIndex << " to " << replacement->debugIndex << std::endl;
            loopHalfedge->startVertex = replacement;
            loopHalfedge = loopHalfedge->previousHalfedge;
            if (nullptr == loopHalfedge)
                break;
            loopHalfedge = loopHalfedge->oppositeHalfedge;
        } while (loopHalfedge != startHalfedge);
    }
}

bool HalfedgeMesh::collapseEdge(Halfedge *halfedge, double maxEdgeLengthSquared)
{
    //std::cout << "=====Try on vertex:" << halfedge->startVertex->debugIndex << " halfedge:" << halfedge->debugIndex << std::endl;
    
    Vector3 collapseTo = (halfedge->startVertex->position +
        halfedge->nextHalfedge->startVertex->position) * 0.5;
    
    //std::cout << "first testLengthSquaredAroundVertex" << std::endl;
    if (testLengthSquaredAroundVertex(halfedge->startVertex, collapseTo, maxEdgeLengthSquared))
        return false;
    //std::cout << "second testLengthSquaredAroundVertex" << std::endl;
    if (testLengthSquaredAroundVertex(halfedge->nextHalfedge->startVertex, collapseTo, maxEdgeLengthSquared))
        return false;

    halfedge->nextHalfedge->startVertex->position = collapseTo;
    
    if (halfedge->previousHalfedge == halfedge->previousHalfedge->startVertex->firstHalfedge) {
        //std::cout << "changeVertexStartHalfedgeFrom vertex:" << halfedge->previousHalfedge->startVertex->debugIndex << " from " << halfedge->previousHalfedge->debugIndex;
        changeVertexStartHalfedgeFrom(halfedge->previousHalfedge->startVertex, 
            halfedge->previousHalfedge);
        //std::cout << " to " << halfedge->previousHalfedge->debugIndex << std::endl;
    }
    halfedge->leftFace->removed = true;
    //std::cout << "pointerVertexToNewVertex from " << halfedge->startVertex->debugIndex << " to " << halfedge->nextHalfedge->startVertex->debugIndex << std::endl;
    pointerVertexToNewVertex(halfedge->startVertex, 
        halfedge->nextHalfedge->startVertex);
    //std::cout << "linkHalfedgePair:" << halfedge->previousHalfedge->oppositeHalfedge->debugIndex << "," << halfedge->nextHalfedge->oppositeHalfedge->debugIndex << std::endl;
    linkHalfedgePair(halfedge->previousHalfedge->oppositeHalfedge,
        halfedge->nextHalfedge->oppositeHalfedge);
    
    Halfedge *opposite = halfedge->oppositeHalfedge;
    if (nullptr != opposite) {
        if (opposite->previousHalfedge == opposite->previousHalfedge->startVertex->firstHalfedge) {
            //std::cout << "    changeVertexStartHalfedgeFrom vertex:" << opposite->previousHalfedge->startVertex->debugIndex << " from " << opposite->previousHalfedge->debugIndex;
            changeVertexStartHalfedgeFrom(opposite->previousHalfedge->startVertex,
                opposite->previousHalfedge);
            //std::cout << "      to " << opposite->previousHalfedge->debugIndex << std::endl;
        }
        changeVertexStartHalfedgeFrom(opposite->startVertex,
            opposite);
        opposite->leftFace->removed = true;
        //std::cout << "linkHalfedgePair:" << opposite->previousHalfedge->oppositeHalfedge->debugIndex << "," << opposite->nextHalfedge->oppositeHalfedge->debugIndex << std::endl;
        linkHalfedgePair(opposite->previousHalfedge->oppositeHalfedge,
            opposite->nextHalfedge->oppositeHalfedge);
    }
    
    //std::cout << "Collapsed on vertex:" << halfedge->startVertex->debugIndex << std::endl;
    
    return true;
}