// Based on: Spatial Splits in Bounding Volume Hierarchies, Stich et. al.
// https://www.nvidia.in/docs/IO/77714/sbvh.pdf
#include <numeric>
#include <thread>
#include "BVH.h"

namespace Atlas {

    namespace Volume {

        uint32_t BVHBuilder::maxDepth = 0;
        uint32_t BVHBuilder::maxTriangles = 0;
        uint32_t BVHBuilder::minTriangles = 10000;
        uint32_t BVHBuilder::spatialSplitCount = 0;
        float BVHBuilder::totalSurfaceArea = 0.0f;

        BVH::BVH(std::vector<AABB>& aabbs, std::vector<BVHTriangle>& data) {

            auto start = clock();

            if (aabbs.size() != data.size())
                return;

            std::vector<BVHBuilder::Ref> refs(data.size());
            for (size_t i = 0; i < refs.size(); i++) {
                refs[i].idx = uint32_t(i);
                refs[i].aabb = aabbs[i];
            }

            // Calculate initial aabb of root
            AABB aabb(glm::vec3(std::numeric_limits<float>::max()),
                glm::vec3(-std::numeric_limits<float>::max()));
            for (auto& ref : refs)
                aabb.Grow(aabbs[ref.idx]);

            auto minOverlap = aabb.GetSurfaceArea() * 10e-5f;
            auto builder = new BVHBuilder(aabb, refs, 0, minOverlap);
            refs.clear();
            refs.reserve(data.size());

            builder->Build(data);

            builder->Flatten(nodes, refs);

            this->aabbs.resize(refs.size());
            this->data.resize(refs.size());

            for (size_t i = 0; i < refs.size(); i++) {
                auto ref = refs[i];
                this->aabbs[i] = aabbs[ref.idx];
                this->data[i] = data[ref.idx];
            }

            delete builder;

        }

        bool BVH::GetIntersection(Ray ray, BVHTriangle& closest, glm::vec3& intersection) {

            constexpr auto max = std::numeric_limits<float>::max();

            return GetIntersection(ray, closest, intersection, max);

        }

        bool BVH::GetIntersection(Ray ray, BVHTriangle& closest, glm::vec3& intersection, float max) {

            intersection.x = max;

            if (nodes.size()) {
                // Use a stack based iterative ray traversal
                uint32_t stack[64];
                stack[0] = 0;

                uint32_t stackPtr = 1;

                while (stackPtr != 0u) {
                    auto& node = nodes[stack[--stackPtr]];

                    if (node.leaf.dataCount & 0x80000000) {
                        // Extract triangle offset and count of the current node
                        auto offset = node.leaf.dataOffset;
                        auto count = node.leaf.dataCount & 0x7fffffff;
                        // Intersect the ray with each triangle
                        for (uint32_t i = offset; i < offset + count; i++) {
                            auto& triangle = data[i];
                            glm::vec3 intersect;
                            if (ray.Intersects(triangle.v0, triangle.v1, triangle.v2, intersect)) {
                                // Only allow intersections "in range"
                                if (intersect.x < intersection.x) {
                                    closest = triangle;
                                    intersection = intersect;
                                }
                            }
                        }
                    }
                    else {
                        float hitL = max, hitR = max;
                        bool intersectR = false, intersectL = false;

                        auto childLPtr = node.inner.leftChild;
                        if (childLPtr) {
                            auto& nodeL = nodes[childLPtr];
                            intersectL = ray.Intersects(
                                nodeL.aabb, 0.0f, intersection.x, hitL);
                        }

                        auto childRPtr = node.inner.rightChild;
                        if (childRPtr) {
                            auto& nodeR = nodes[childRPtr];
                            intersectR = ray.Intersects(
                                nodeR.aabb, 0.0f, intersection.x, hitR);
                        }

                        if (intersectR && intersectL) {
                            if (hitL < hitR) {
                                stack[stackPtr++] = childRPtr;
                                stack[stackPtr++] = childLPtr;
                            }
                            else {
                                stack[stackPtr++] = childLPtr;
                                stack[stackPtr++] = childRPtr;
                            }
                        }
                        else if (intersectR && !intersectL) {
                            stack[stackPtr++] = childRPtr;
                        }
                        else if (!intersectR && intersectL) {
                            stack[stackPtr++] = childLPtr;
                        }
                    }
                }


            }

            return (intersection.x < max);

        }

        bool BVH::GetIntersectionAny(Ray ray, float max) {

            if (nodes.size()) {
                // Use a stack based iterative ray traversal
                uint32_t stack[64];
                stack[0] = 0;

                uint32_t stackPtr = 1;

                do {
                    // Pop the last node from the stack
                    auto& node = nodes[stack[--stackPtr]];
                    auto& aabb = node.aabb;

                    // Check whether the ray intersects the node
                    if (!ray.Intersects(aabb, 0.0f, max))
                        continue;

                    // Check if the node is a leaf node
                    // indicated by the most significant bit.
                    if (node.leaf.dataCount & 0x80000000) {
                        // Extract triangle offset and count of the current node
                        auto offset = node.leaf.dataOffset;
                        auto count = node.leaf.dataCount & 0x7fffffff;
                        // Intersect the ray with each triangle
                        for (uint32_t i = offset; i < offset + count; i++) {
                            auto& triangle = data[i];
                            glm::vec3 intersect;
                            if (ray.Intersects(triangle.v0, triangle.v1, triangle.v2, intersect)) {
                                // Only allow intersections "in range"
                                if (intersect.x < max) {
                                    return true;
                                }
                            }
                        }
                    }
                    else {
                        // Only push child if the child index is valid
                        // indicated by an index larger than zero
                        if (node.inner.leftChild)
                            stack[stackPtr++] = node.inner.leftChild;
                        if (node.inner.rightChild)
                            stack[stackPtr++] = node.inner.rightChild;
                    }
                } while (stackPtr != 0);
            }

            return false;

        }

        std::vector<BVHNode>& BVH::GetTree() {

            return nodes;

        }

        BVHBuilder::BVHBuilder(const AABB aabb, const std::vector<Ref> refs,
            uint32_t depth, const float minOverlap, const uint32_t binCount) :
            depth(depth), binCount(binCount), minOverlap(minOverlap), aabb(aabb), refs(refs) {

            // Calculate cost for current node
            nodeCost = float(refs.size()) * this->aabb.GetSurfaceArea();

            totalSurfaceArea += this->aabb.GetSurfaceArea();

        }

        BVHBuilder::~BVHBuilder() {

            delete leftChild;
            delete rightChild;

        }

        void BVHBuilder::Build(const std::vector<BVHTriangle>& data) {

            const size_t refCount = 4;
            // Create leaf node
            if (refs.size() <= refCount || depth >= 32) {
                CreateLeaf();
                return;
            }

            Split objectSplit;
            objectSplit = FindObjectSplit();

            Split spatialSplit;
            if (depth <= 16) {
                AABB overlap = objectSplit.leftAABB;
                overlap.Intersect(objectSplit.rightAABB);
                if (overlap.GetSurfaceArea() >= minOverlap) {
                    spatialSplit = FindSpatialSplit(data);
                }
            }

            std::vector<Ref> leftRefs;
            std::vector<Ref> rightRefs;

            leftRefs.reserve(refs.size() / 2);
            rightRefs.reserve(refs.size() / 2);

            Split split;
            // If we haven't found a cost improvement we create a leaf node
            if ((objectSplit.axis < 0 || objectSplit.cost >= nodeCost) &&
                (spatialSplit.axis < 0 || spatialSplit.cost >= nodeCost)) {
                if (refs.size() >= 2 * refCount) {
                    split = PerformMedianSplit(rightRefs, leftRefs);
                }
                else {
                    CreateLeaf();
                    return;
                }
            }
            else {
                if (spatialSplit.cost < objectSplit.cost) {
                    PerformSpatialSplit(data, rightRefs, leftRefs, spatialSplit);
                    split = spatialSplit;
                }

                if (objectSplit.cost <= spatialSplit.cost) {
                    leftRefs.clear();
                    rightRefs.clear();
                    PerformObjectSplit(rightRefs, leftRefs, objectSplit);
                    split = objectSplit;
                }
                else {
                    spatialSplitCount++;
                }
            }

            refs.clear();
            refs.shrink_to_fit();

            if (depth <= 2) {
                std::thread leftBuilderThread, rightBuilderThread;

                auto leftLambda = [&]() {
                    if (!leftRefs.size()) return;
                    leftChild = new BVHBuilder(split.leftAABB, leftRefs, depth + 1, minOverlap);
                    leftChild->Build(data);
                };

                auto rightLambda = [&]() {
                    if (!rightRefs.size()) return;
                    rightChild = new BVHBuilder(split.rightAABB, rightRefs, depth + 1, minOverlap);
                    rightChild->Build(data);
                };

                leftBuilderThread = std::thread{ leftLambda };
                rightBuilderThread = std::thread{ rightLambda };

                leftBuilderThread.join();
                rightBuilderThread.join();
            }
            else {
                if (leftRefs.size()) {
                    leftChild = new BVHBuilder(split.leftAABB, leftRefs, depth + 1, minOverlap);
                    leftChild->Build(data);
                }

                if (rightRefs.size()) {
                    rightChild = new BVHBuilder(split.rightAABB, rightRefs, depth + 1, minOverlap);
                    rightChild->Build(data);
                }
            }


        }

        void BVHBuilder::Flatten(std::vector<BVHNode>& nodes, std::vector<Ref>& refs) {

            const auto nodeIdx = nodes.size();
            nodes.push_back(BVHNode());
            // Check for leaf
            if (this->refs.size()) {
                nodes[nodeIdx].leaf.dataOffset = uint32_t(refs.size());
                nodes[nodeIdx].leaf.dataCount = 0x80000000 | uint32_t(this->refs.size());

                refs.insert(refs.end(), this->refs.begin(), this->refs.end());
            }
            else {
                // Flatten recursively
                if (leftChild) {
                    nodes[nodeIdx].inner.leftChild = uint32_t(nodes.size());
                    leftChild->Flatten(nodes, refs);
                }
                if (rightChild) {
                    nodes[nodeIdx].inner.rightChild = uint32_t(nodes.size());
                    rightChild->Flatten(nodes, refs);
                }
            }

            nodes[nodeIdx].aabb = aabb;

        }


        BVHBuilder::Split BVHBuilder::FindObjectSplit() {

            Split split;
            const auto depthBinCount = std::max(binCount / (depth + 1), 16u);

            std::vector<Bin> bins(depthBinCount);
            std::vector<AABB> rightAABBs(bins.size());

            // Iterate over 3 axises
            for (int32_t i = 0; i < 3; i++) {

                for (auto& bin : bins) {
                    bin.aabb = InitialAABB();
                    bin.primitiveCount = 0;
                }

                for (auto& aabb : rightAABBs) {
                    aabb = InitialAABB();
                }

                auto start = aabb.min[i];
                auto stop = aabb.max[i];

                // If the dimension of this axis is to small continue
                if (fabsf(stop - start) < 1e-3f)
                    continue;

                auto binSize = (stop - start) / float(depthBinCount);
                auto invBinSize = 1.0f / binSize;

                for (const auto& ref : refs) {
                    const auto value = 0.5f * (ref.aabb.min[i] + ref.aabb.max[i]);

                    auto binIdx = uint32_t(glm::clamp((value - start) * invBinSize,
                        0.0f, float(depthBinCount) - 1.0f));

                    bins[binIdx].primitiveCount++;
                    bins[binIdx].aabb.Grow(ref.aabb);
                }

                // Sweep from right to left
                AABB rightAABB = InitialAABB();
                for (size_t j = bins.size() - 1; j > 0; j--) {
                    rightAABB.Grow(bins[j].aabb);
                    rightAABBs[j - 1] = rightAABB;
                }

                AABB leftAABB = InitialAABB();
                uint32_t primitivesLeft = 0;

                // Sweep from left to right and attempt to
                // find cost improvement
                for (size_t j = 1; j < bins.size(); j++) {
                    leftAABB.Grow(bins[j - 1].aabb);
                    primitivesLeft += bins[j - 1].primitiveCount;

                    const auto rightAABB = rightAABBs[j - 1];
                    const auto primitivesRight = uint32_t(refs.size()) - primitivesLeft;

                    if (!primitivesLeft || !primitivesRight) continue;

                    const auto leftSurface = leftAABB.GetSurfaceArea();
                    const auto rightSurface = rightAABB.GetSurfaceArea();

                    // Calculate cost for current split
                    const auto cost = leftSurface * float(primitivesLeft) +
                        rightSurface * float(primitivesRight);

                    // Check if cost has improved
                    if (cost < split.cost) {
                        split.cost = cost;
                        split.axis = i;
                        split.binIdx = uint32_t(j);
                        split.pos = start + float(j) * binSize;

                        split.leftAABB = leftAABB;
                        split.rightAABB = rightAABB;
                    }
                }

            }

            return split;

        }

        void BVHBuilder::PerformObjectSplit(std::vector<Ref>& rightRefs,
            std::vector<Ref>& leftRefs, Split& split) {

            const auto depthBinCount = std::max(binCount / (depth + 1), 16u);

            auto start = aabb.min[split.axis];
            auto stop = aabb.max[split.axis];

            auto binSize = (stop - start) / float(depthBinCount);
            auto invBinSize = 1.0f / binSize;

            for (const auto& ref : refs) {
                const auto value = 0.5f * (ref.aabb.min[split.axis]
                    + ref.aabb.max[split.axis]);

                auto binIdx = uint32_t(glm::clamp((value - start) * invBinSize,
                    0.0f, float(depthBinCount) - 1.0f));

                if (binIdx < split.binIdx) {
                    leftRefs.push_back(ref);
                }
                else {
                    rightRefs.push_back(ref);
                }
            }

        }

        BVHBuilder::Split BVHBuilder::FindSpatialSplit(const std::vector<BVHTriangle>& data) {

            Split split;
            const auto depthBinCount = std::max(binCount / (depth + 1), 16u);

            std::vector<Bin> bins(depthBinCount);
            std::vector<AABB> rightAABBs(bins.size());

            // Iterate over 3 axises
            for (int32_t i = 0; i < 3; i++) {

                for (auto& bin : bins) {
                    bin.aabb = InitialAABB();
                    bin.enter = 0;
                    bin.exit = 0;
                }

                for (auto& aabb : rightAABBs) {
                    aabb = InitialAABB();
                }

                auto start = aabb.min[i];
                auto stop = aabb.max[i];

                // If the dimension of this axis is to small continue
                if (fabsf(stop - start) < 1e-3f)
                    continue;

                auto binSize = (stop - start) / float(depthBinCount);
                auto invBinSize = 1.0f / binSize;

                for (const auto& ref : refs) {
                    const auto startBinIdx = uint32_t(glm::clamp((ref.aabb.min[i] - start) * invBinSize,
                        0.0f, float(depthBinCount) - 1.0f));
                    const auto endBinIdx = uint32_t(glm::clamp((ref.aabb.max[i] - start) * invBinSize,
                        0.0f, float(depthBinCount) - 1.0f));

                    // If the reference only is in a single bin
                    if (startBinIdx == endBinIdx) {
                        bins[startBinIdx].enter++;
                        bins[startBinIdx].exit++;
                        bins[startBinIdx].aabb.Grow(ref.aabb);
                        continue;
                    }

                    Ref currentRef = ref;
                    // Split the reference across multiple bins
                    for (uint32_t j = startBinIdx; j < endBinIdx; j++) {
                        Ref leftRef, rightRef;
                        const auto planePos = start + float(j + 1) * binSize;
                        const auto triangle = data[ref.idx];
                        SplitReference(triangle, currentRef,
                            leftRef, rightRef, planePos, i);
                        bins[j].aabb.Grow(leftRef.aabb);
                        currentRef = rightRef;
                    }

                    // Grow last right bin which isn't covered by the loop
                    bins[endBinIdx].aabb.Grow(currentRef.aabb);
                    bins[startBinIdx].enter++;
                    bins[endBinIdx].exit++;
                }

                // Sweep from right to left
                AABB rightAABB = InitialAABB();
                for (size_t j = bins.size() - 1; j > 0; j--) {
                    rightAABB.Grow(bins[j].aabb);
                    rightAABBs[j - 1] = rightAABB;
                }

                AABB leftAABB = InitialAABB();
                uint32_t primitivesRight = uint32_t(refs.size());
                uint32_t primitivesLeft = 0;

                // Sweep from left to right and attempt to
                // find cost improvement
                for (size_t j = 1; j < bins.size(); j++) {
                    leftAABB.Grow(bins[j - 1].aabb);

                    primitivesLeft += bins[j - 1].enter;
                    primitivesRight -= bins[j - 1].exit;

                    const auto rightAABB = rightAABBs[j - 1];

                    if (!primitivesLeft || !primitivesRight) continue;

                    const auto leftSurface = leftAABB.GetSurfaceArea();
                    const auto rightSurface = rightAABB.GetSurfaceArea();

                    // Calculate cost for current split
                    const auto cost = leftSurface * float(primitivesLeft) +
                        rightSurface * float(primitivesRight);

                    // Check if cost has improved
                    if (cost < split.cost) {
                        split.cost = cost;
                        split.axis = i;
                        split.binIdx = uint32_t(j);
                        split.pos = start + float(j) * binSize;

                        split.leftAABB = leftAABB;
                        split.rightAABB = rightAABB;
                    }
                }

            }

            return split;

        }

        void BVHBuilder::PerformSpatialSplit(const std::vector<BVHTriangle>& data,
            std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs, Split& split) {

            const auto depthBinCount = std::max(binCount / (depth + 1), 16u);

            auto start = aabb.min[split.axis];
            auto stop = aabb.max[split.axis];

            auto binSize = (stop - start) / float(depthBinCount);
            auto invBinSize = 1.0f / binSize;

            for (const auto& ref : refs) {
                const auto min = ref.aabb.min[split.axis];
                const auto max = ref.aabb.max[split.axis];

                const auto startBinIdx = uint32_t(glm::clamp((min - start) * invBinSize,
                    0.0f, float(depthBinCount) - 1.0f));
                const auto endBinIdx = uint32_t(glm::clamp((max - start) * invBinSize,
                    0.0f, float(depthBinCount) - 1.0f));

                if (endBinIdx < split.binIdx) {
                    leftRefs.push_back(ref);
                }
                else if (startBinIdx >= split.binIdx) {
                    rightRefs.push_back(ref);
                }
                else {
                    Ref leftRef, rightRef;
                    SplitReference(data[ref.idx], ref, leftRef,
                        rightRef, split.pos, split.axis);
                    leftRef.idx = ref.idx;
                    rightRef.idx = ref.idx;
                    leftRefs.push_back(leftRef);
                    rightRefs.push_back(rightRef);
                }
            }

        }

        void BVHBuilder::SplitReference(const BVHTriangle triangle, Ref currentRef,
            Ref& leftRef, Ref& rightRef, const float planePos, const int32_t axis) {

            for (uint8_t i = 0; i < 3; i++) {
                glm::vec3 v0, v1;
                switch (i) {
                case 0: v0 = triangle.v0; v1 = triangle.v1; break;
                case 1: v0 = triangle.v1; v1 = triangle.v2; break;
                case 2: v0 = triangle.v2; v1 = triangle.v0; break;
                default: break;
                }

                // Then check for edge between v0 and v1
                if (v0[axis] < planePos && v1[axis] > planePos ||
                    v0[axis] > planePos && v1[axis] < planePos) {
                    // Interpolate on edge and find point at plane position
                    auto off = glm::clamp((planePos - v0[axis]) /
                        (v1[axis] - v0[axis]), 0.0f, 1.0f);
                    auto interpolation = glm::mix(v0, v1, off);

                    leftRef.aabb.Grow(interpolation);
                    rightRef.aabb.Grow(interpolation);
                }

                // First check for single vertex
                if (v0[axis] <= planePos)
                    leftRef.aabb.Grow(v0);
                if (v0[axis] >= planePos)
                    rightRef.aabb.Grow(v0);

            }

            leftRef.aabb.max[axis] = planePos;
            rightRef.aabb.min[axis] = planePos;

            leftRef.aabb.Intersect(currentRef.aabb);
            rightRef.aabb.Intersect(currentRef.aabb);

        }

        BVHBuilder::Split BVHBuilder::PerformMedianSplit(std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs) {

            Split split;

            auto dimensions = aabb.max - aabb.min;
            auto axis = dimensions.x > dimensions.y ? dimensions.x > dimensions.z ? 0 : 2 :
                dimensions.y > dimensions.z ? 1 : 2;
            std::sort(refs.begin(), refs.end(), [&](const Ref& ref0, const Ref& ref1) {
                auto center0 = ref0.aabb.max[axis] - ref0.aabb.min[axis];
                auto center1 = ref1.aabb.max[axis] - ref1.aabb.min[axis];
                return center0 < center1;
                });

            auto splitIdx = uint32_t(refs.size() / 2);

            for (uint32_t i = 0; i < splitIdx; i++) {
                const auto& ref = refs[i];
                split.leftAABB.Grow(ref.aabb);
                leftRefs.push_back(ref);
            }

            for (uint32_t i = splitIdx; i < uint32_t(refs.size()); i++) {
                const auto& ref = refs[i];
                split.rightAABB.Grow(ref.aabb);
                rightRefs.push_back(ref);
            }

            return split;

        }

        void BVHBuilder::CreateLeaf() {

            maxDepth = std::max(depth, maxDepth);
            minTriangles = std::min(uint32_t(refs.size()), minTriangles);
            maxTriangles = std::max(uint32_t(refs.size()), maxTriangles);

        }

        AABB BVHBuilder::InitialAABB() {

            const auto min = glm::vec3(std::numeric_limits<float>::max());
            const auto max = glm::vec3(-std::numeric_limits<float>::max());
            return AABB(min, max);

        }

    }

}