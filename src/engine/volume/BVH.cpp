// Based on: Spatial Splits in Bounding Volume Hierarchies, Stich et. al.
// https://www.nvidia.in/docs/IO/77714/sbvh.pdf
#include <numeric>
#include <future>

#include "BVH.h"
#include "Log.h"
#include "tools/PerformanceCounter.h"

namespace Atlas {

    namespace Volume {

        BVH::BVH(const std::vector<AABB>& aabbs, const std::vector<BVHTriangle>& data, bool parallelBuild) {

            Tools::PerformanceCounter perfCounter;

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

            auto minOverlap = aabb.GetSurfaceArea() * 10e-6f;
            auto builder = new BVHBuilder(aabb, 0, refs.size(), minOverlap, 256);

            JobGroup group { JobPriority::Low };
            builder->Build(refs, data, group, parallelBuild);
            JobSystem::Wait(group);

            refs.reserve(data.size());

            builder->Flatten(nodes, refs);

            this->aabbs.resize(refs.size());
            this->data.resize(refs.size());

            for (size_t i = 0; i < refs.size(); i++) {
                auto ref = refs[i];
                this->aabbs[i] = aabbs[ref.idx];
                this->data[i] = data[ref.idx];
                this->data[i].endOfNode = ref.endOfNode;
            }

            delete builder;

        }

        BVH::BVH(const std::vector<AABB>& aabbs, bool parallelBuild) {

            refs.resize(aabbs.size());
            for (size_t i = 0; i < refs.size(); i++) {
                refs[i].idx = uint32_t(i);
                refs[i].aabb = aabbs[i];
            }

            // Calculate initial aabb of root
            AABB aabb(glm::vec3(std::numeric_limits<float>::max()),
                glm::vec3(-std::numeric_limits<float>::max()));
            for (auto& ref : refs)
                aabb.Grow(aabbs[ref.idx]);

            auto builder = new BVHBuilder(aabb, 0, refs.size(), 64);

            JobGroup group { JobPriority::Medium };
            builder->Build(refs, group, parallelBuild);
            JobSystem::Wait(group);

            refs.reserve(data.size());

            if (!nodes.size() && aabbs.size() == 1) {
                BVHNode node;
                node.leftPtr = ~0;
                node.rightPtr = ~0;
                node.leftAABB = aabb;
                node.rightAABB = AABB(vec3(0.0f), vec3(0.0f));

                nodes.push_back(node);
            }

            builder->Flatten(nodes, refs);

            this->aabbs.resize(refs.size());

            for (size_t i = 0; i < refs.size(); i++) {
                auto ref = refs[i];
                this->aabbs[i] = aabbs[ref.idx];
            }

            delete builder;

        }
        
        bool BVH::GetIntersection(std::vector<std::pair<int32_t, float>>& stack, Ray ray, BVHTriangle& closest, glm::vec3& intersection) {

            intersection.x = ray.tMax;

            if (nodes.size()) {
                // Use a stack based iterative ray traversal
                stack[0] = std::pair(0, 0.0f);

                uint32_t stackPtr = 1u;
                int32_t closestPtr = 0;

                while (stackPtr != 0u) {
                    const auto [nodePtr, distance] = stack[--stackPtr];

                    if (distance > intersection.x) continue;

                    if (nodePtr < 0) {
                        auto triPtr = ~nodePtr;
                        auto endOfNode = false;
                        // Intersect the ray with each triangle
                        while (!endOfNode) {
                            auto ptr = triPtr++;
                            auto& triangle = data[ptr];
                            endOfNode = triangle.endOfNode;

                            glm::vec3 intersect;
                            bool hit = ray.Intersects(triangle.v0, triangle.v1, triangle.v2, intersect);

                            // Only allow intersections "in range"
                            if (hit && intersect.x < intersection.x) {
                                closestPtr = ptr;
                                intersection = intersect;
                            }
                        }
                    }
                    else {
                        float hitL = 0.0f, hitR = 0.0f;

                        ray.tMax = intersection.x;

                        const auto& node = nodes[nodePtr];
                        // We might want to check whether these nodes are leafs
                        auto intersectL = ray.Intersects(node.leftAABB, hitL);
                        auto intersectR = ray.Intersects(node.rightAABB, hitR);

                        if (intersectR && intersectL) {
                            if (hitL < hitR) {
                                stack[stackPtr++] = std::pair(node.rightPtr, hitR);
                                stack[stackPtr++] = std::pair(node.leftPtr, hitL);
                            }
                            else {
                                stack[stackPtr++] = std::pair(node.leftPtr, hitL);
                                stack[stackPtr++] = std::pair(node.rightPtr, hitR);
                            }
                        }
                        else if (intersectR && !intersectL) {
                            stack[stackPtr++] = std::pair(node.rightPtr, hitR);
                        }
                        else if (!intersectR && intersectL) {
                            stack[stackPtr++] = std::pair(node.leftPtr, hitL);
                        }

                    }

                }

                closest = data[closestPtr];
            }

            return (intersection.x < ray.tMax);

        }

        bool BVH::GetIntersectionAny(std::vector<std::pair<int32_t, float>>& stack, Ray ray) {

            if (nodes.size()) {
                stack[0] = std::pair(0, 0.0f);

                uint32_t stackPtr = 1u;

                while (stackPtr != 0u) {
                    const auto [nodePtr, _] = stack[--stackPtr];

                    if (nodePtr < 0) {
                        auto triPtr = ~nodePtr;
                        auto endOfNode = false;
                        // Intersect the ray with each triangle
                        while (!endOfNode) {
                            auto& triangle = data[triPtr++];
                            endOfNode = triangle.endOfNode;

                            glm::vec3 intersect;
                            bool hit = ray.Intersects(triangle.v0, triangle.v1, triangle.v2, intersect);

                            // Only allow intersections "in range"
                            if (hit && intersect.x < ray.tMax) {
                                return true;
                            }
                        }
                    }
                    else {
                        const auto& node = nodes[nodePtr];
                        // We might want to check whether these nodes are leafs
                        auto intersectL = ray.Intersects(node.leftAABB);
                        auto intersectR = ray.Intersects(node.rightAABB);

                        if (intersectR) stack[stackPtr++] = std::pair(node.rightPtr, 0.0f);
                        if (intersectL) stack[stackPtr++] = std::pair(node.leftPtr, 0.0f);
                    }
                }
            }

            return false;

        }

        std::vector<BVHNode>& BVH::GetTree() {

            return nodes;

        }

        BVHBuilder::BVHBuilder(const AABB aabb, uint32_t depth, size_t refCount,
            const float minOverlap, const uint32_t binCount) :
            depth(depth), binCount(binCount), minOverlap(minOverlap), aabb(aabb) {

            // Calculate cost for current node
            nodeCost = float(refCount) * this->aabb.GetSurfaceArea();

        }

        BVHBuilder::BVHBuilder(const AABB aabb, uint32_t depth, size_t refCount, const uint32_t binCount) :
            depth(depth), binCount(binCount), minOverlap(0.0f), aabb(aabb) {

            // Calculate cost for current node
            nodeCost = float(refCount) * this->aabb.GetSurfaceArea();

        }

        BVHBuilder::~BVHBuilder() {

            delete leftChild;
            delete rightChild;

        }

        void BVHBuilder::Build(std::vector<Ref>& refs, const std::vector<BVHTriangle>& data, JobGroup& jobGroup, bool parallelBuild) {

            const size_t refCount = 2;
            // Create leaf node
            if ((refs.size() <= refCount || depth >= 32) && depth > 0) {
                CreateLeaf(refs);
                return;
            }

            Split objectSplit;
            objectSplit = FindObjectSplit(refs);

            Split spatialSplit;
            if (depth <= 16) {
                AABB overlap = objectSplit.leftAABB;
                overlap.Intersect(objectSplit.rightAABB);
                if (overlap.GetSurfaceArea() >= minOverlap) {
                    spatialSplit = FindSpatialSplit(refs, data);
                }
            }

            std::vector<Ref> leftRefs;
            std::vector<Ref> rightRefs;

            leftRefs.reserve(refs.size());
            rightRefs.reserve(refs.size());

            Split split;
            // If we haven't found a cost improvement we create a leaf node
            if ((objectSplit.axis < 0 || objectSplit.cost >= nodeCost) &&
                (spatialSplit.axis < 0 || spatialSplit.cost >= nodeCost)) {
                // In the rare case there was not enough improvements with just a few triangles
                if (depth == 0) {
                    split = PerformMedianSplit(refs, rightRefs, leftRefs);
                }
                else {
                    CreateLeaf(refs);
                    return;
                }
            }
            else {
                if (spatialSplit.cost < objectSplit.cost) {
                    PerformSpatialSplit(refs, data, rightRefs, leftRefs, spatialSplit);
                    split = spatialSplit;
                }

                if (objectSplit.cost <= spatialSplit.cost) {
                    leftRefs.clear();
                    rightRefs.clear();
                    PerformObjectSplit(refs, rightRefs, leftRefs, objectSplit);
                    split = objectSplit;
                }
            }

            // Last resort. This could happend due to spatial unsplitting
            if (leftRefs.empty() || rightRefs.empty())
                CreateLeaf(refs);

            refs.clear();
            refs.shrink_to_fit();

            if (depth <= 6 && parallelBuild) {
                auto leftRefSize = leftRefs.size(), rightRefSize = rightRefs.size();
                auto leftLambda = [=, &jobGroup, leftRefs = std::move(leftRefs)](JobData&) {
                    auto refs = std::move(leftRefs);
                    leftChild = new BVHBuilder(split.leftAABB, depth + 1, refs.size(), binCount);
                    leftChild->Build(refs, jobGroup, parallelBuild);
                };

                auto rightLambda = [=, &jobGroup, rightRefs = std::move(rightRefs)](JobData&) {
                    auto refs = std::move(rightRefs);
                    rightChild = new BVHBuilder(split.rightAABB, depth + 1, refs.size(), binCount);
                    rightChild->Build(refs, jobGroup, parallelBuild);
                };

                if (leftRefSize > 0)
                    JobSystem::Execute(jobGroup, leftLambda);
                if (rightRefSize > 0)
                    JobSystem::Execute(jobGroup, rightLambda);
            }   
            else {
                if (leftRefs.size()) {
                    leftChild = new BVHBuilder(split.leftAABB, depth + 1, leftRefs.size(), binCount);
                    leftChild->Build(leftRefs, jobGroup, parallelBuild);
                }

                if (rightRefs.size()) {
                    rightChild = new BVHBuilder(split.rightAABB, depth + 1, rightRefs.size(), binCount);
                    rightChild->Build(rightRefs, jobGroup, parallelBuild);
                }
            }

        }

        void BVHBuilder::Build(std::vector<Ref>& refs, JobGroup& jobGroup, bool parallelBuild) {

            // Create leaf node
            if (refs.size() == 1) {
                CreateLeaf(refs);
                return;
            }

            Split objectSplit;
            objectSplit = FindObjectSplit(refs);

            std::vector<Ref> leftRefs;
            std::vector<Ref> rightRefs;

            leftRefs.reserve(refs.size() / 2);
            rightRefs.reserve(refs.size() / 2);

            Split split;
            // If we haven't found a cost improvement but need to continue since only one item per leaf is allowed
            if (objectSplit.axis < 0 || objectSplit.cost >= nodeCost) {
                split = PerformMedianSplit(refs, rightRefs, leftRefs);
            }
            else {
                leftRefs.clear();
                rightRefs.clear();
                PerformObjectSplit(refs, rightRefs, leftRefs, objectSplit);
                split = objectSplit;
            }

            refs.clear();
            refs.shrink_to_fit();

            if (depth <= 8 && parallelBuild) {
                auto leftRefSize = leftRefs.size(), rightRefSize = rightRefs.size();
                auto leftLambda = [=, &jobGroup, leftRefs = std::move(leftRefs)](JobData&) {
                    auto refs = std::move(leftRefs);
                    leftChild = new BVHBuilder(split.leftAABB, depth + 1, refs.size(), binCount);
                    leftChild->Build(refs, jobGroup, parallelBuild);
                };

                auto rightLambda = [=, &jobGroup, rightRefs = std::move(rightRefs)](JobData&) {
                    auto refs = std::move(rightRefs);
                    rightChild = new BVHBuilder(split.rightAABB, depth + 1, refs.size(), binCount);
                    rightChild->Build(refs, jobGroup, parallelBuild);
                };

                if (leftRefSize > 0)
                    JobSystem::Execute(jobGroup, leftLambda);
                if (rightRefSize > 0)
                    JobSystem::Execute(jobGroup, rightLambda);
            }   
            else {
                if (leftRefs.size()) {
                    leftChild = new BVHBuilder(split.leftAABB, depth + 1, leftRefs.size(), binCount);
                    leftChild->Build(leftRefs, jobGroup, parallelBuild);
                }

                if (rightRefs.size()) {
                    rightChild = new BVHBuilder(split.rightAABB, depth + 1, rightRefs.size(), binCount);
                    rightChild->Build(rightRefs, jobGroup, parallelBuild);
                }
            }

        }

        void BVHBuilder::Flatten(std::vector<BVHNode>& nodes, std::vector<Ref>& refs) {

            // Check for leaf
            if (this->refs.size()) {
                for (auto& ref : this->refs) {
                    ref.nodeIdx = uint32_t(nodes.size() - 1);
                }
                refs.insert(refs.end(), this->refs.begin(), this->refs.end());
                refs[refs.size() - 1].endOfNode = true;
            }
            else {
                const auto nodeIdx = nodes.size();
                nodes.push_back(BVHNode());

                // Reorder such that shadow rays hit large surface are first
                if (leftChild->aabb.GetSurfaceArea() < rightChild->aabb.GetSurfaceArea())
                    std::swap(leftChild, rightChild);

                // Flatten recursively
                if (leftChild) {
                    auto leaf = leftChild->refs.size() > 0;
                    nodes[nodeIdx].leftAABB = leftChild->aabb;
                    nodes[nodeIdx].leftPtr = leaf ? ~int32_t(refs.size()) : uint32_t(nodes.size());
                    leftChild->Flatten(nodes, refs);
                }
                if (rightChild) {
                    auto leaf = rightChild->refs.size() > 0;
                    nodes[nodeIdx].rightAABB = rightChild->aabb;
                    nodes[nodeIdx].rightPtr = leaf ? ~int32_t(refs.size()) : uint32_t(nodes.size());
                    rightChild->Flatten(nodes, refs);
                }
            }

        }


        BVHBuilder::Split BVHBuilder::FindObjectSplit(std::vector<Ref>& refs) {

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

        void BVHBuilder::PerformObjectSplit(std::vector<Ref>& refs, std::vector<Ref>& rightRefs,
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

        BVHBuilder::Split BVHBuilder::FindSpatialSplit(std::vector<Ref>& refs, const std::vector<BVHTriangle>& data) {

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

        void BVHBuilder::PerformSpatialSplit(std::vector<Ref>& refs, const std::vector<BVHTriangle>& data,
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
                    split.leftAABB.Grow(ref.aabb);
                }
                else if (startBinIdx >= split.binIdx) {
                    rightRefs.push_back(ref);
                    split.rightAABB.Grow(ref.aabb);
                }
            }

            for (const auto& ref : refs) {
                const auto min = ref.aabb.min[split.axis];
                const auto max = ref.aabb.max[split.axis];

                const auto startBinIdx = uint32_t(glm::clamp((min - start) * invBinSize,
                    0.0f, float(depthBinCount) - 1.0f));
                const auto endBinIdx = uint32_t(glm::clamp((max - start) * invBinSize,
                    0.0f, float(depthBinCount) - 1.0f));

                if (endBinIdx >= split.binIdx && startBinIdx < split.binIdx) {
                    Ref leftRef, rightRef;
                    SplitReference(data[ref.idx], ref, leftRef,
                        rightRef, split.pos, split.axis);

                    auto unsplitLeft = split.leftAABB;
                    auto unsplitRight = split.rightAABB;
                    auto duplicateLeft = split.leftAABB;
                    auto duplicateRight = split.rightAABB;

                    unsplitLeft.Grow(ref.aabb);
                    unsplitRight.Grow(ref.aabb);
                    duplicateLeft.Grow(leftRef.aabb);
                    duplicateRight.Grow(rightRef.aabb);

                    // Original triangle cost
                    auto leftRefCost = float(leftRefs.size());
                    auto rightRefCost = float(rightRefs.size());
                    // Triangle cost with added reference to either right or left side
                    auto leftModRefCost = float(leftRefs.size() + 1);
                    auto rightModRefCost = float(rightRefs.size() + 1);

                    auto unsplitLeftSAH = unsplitLeft.GetSurfaceArea() * leftModRefCost +
                        split.rightAABB.GetSurfaceArea() * rightRefCost;
                    auto unsplitRightSAH = split.leftAABB.GetSurfaceArea() * leftRefCost +
                        unsplitRight.GetSurfaceArea() * rightModRefCost;
                    auto duplicateSAH = duplicateLeft.GetSurfaceArea() * leftModRefCost +
                        duplicateRight.GetSurfaceArea() * rightModRefCost;

                    auto minSAH = glm::min(duplicateSAH, glm::min(unsplitRightSAH, unsplitLeftSAH));

                    if (minSAH == unsplitLeftSAH) {
                        split.leftAABB = unsplitLeft;
                        leftRefs.push_back(ref);
                    }
                    else if (minSAH == unsplitRightSAH) {
                        split.rightAABB = unsplitRight;
                        rightRefs.push_back(ref);
                    }
                    else {
                        leftRef.idx = ref.idx;
                        rightRef.idx = ref.idx;
                        split.leftAABB = duplicateLeft;
                        split.rightAABB = duplicateRight;
                        leftRefs.push_back(leftRef);
                        rightRefs.push_back(rightRef);
                    }
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

        BVHBuilder::Split BVHBuilder::PerformMedianSplit(std::vector<Ref>& refs,
            std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs) {

            Split split;

            auto dimensions = aabb.max - aabb.min;

            auto axis = 0;
            axis = dimensions.y > dimensions[axis] ? 1 : axis;
            axis = dimensions.z > dimensions[axis] ? 2 : axis;

            auto splitCutoff = aabb.min[axis] + dimensions[axis] / 2.0f;

            for (auto& ref : refs) {
                auto center = (ref.aabb.max[axis] - ref.aabb.min[axis]) * 0.5f + ref.aabb.min[axis];
                if (center < splitCutoff) {
                    split.leftAABB.Grow(ref.aabb);
                    leftRefs.push_back(ref);
                }
                else {
                    split.rightAABB.Grow(ref.aabb);
                    rightRefs.push_back(ref);
                }
            }

            // Use this as a fallback, otherwise we might get endless recursions
            if (!leftRefs.size() || !rightRefs.size()) {
                split = Split();

                leftRefs.clear();
                rightRefs.clear();

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
            }

            return split;

        }

        void BVHBuilder::CreateLeaf(std::vector<Ref>& refs) {

            this->refs = refs;

        }

        AABB BVHBuilder::InitialAABB() {

            const auto min = glm::vec3(std::numeric_limits<float>::max());
            const auto max = glm::vec3(-std::numeric_limits<float>::max());
            return AABB(min, max);

        }

    }

}