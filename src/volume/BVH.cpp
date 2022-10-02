// Based on: Spatial Splits in Bounding Volume Hierarchies, Stich et. al.
// https://www.nvidia.in/docs/IO/77714/sbvh.pdf
#include <numeric>
#include <thread>

#include "BVH.h"
#include "Log.h"

namespace Atlas {

    namespace Volume {

        BVHBuilder::BVHBuilder(const BVHTriangle* data, size_t dataCount) : data(data) {

            node = new Node(2);

            node->depth = 0;
            node->aabb = InitialAABB();
            node->refs.reserve(dataCount);

            for (size_t i = 0; i < dataCount; i++) {
                auto& triangle = data[i];

                Ref ref;

                ref.idx = uint32_t(i);
                ref.aabb.Grow(triangle.v0);
                ref.aabb.Grow(triangle.v1);
                ref.aabb.Grow(triangle.v2);

                node->aabb.Grow(ref.aabb);

                node->refs.push_back(ref);
            }

        }

        void BVHBuilder::Build() {

            minOverlap = node->aabb.GetSurfaceArea() * settings.minOverlap;

            binCount = settings.binCount;

            auto contextBinCount = settings.quality == HIGH ? uint32_t(node->refs.size()) : binCount;
            ThreadContext context(contextBinCount, node->refs.size());
            Build(context, node);

        }

        void BVHBuilder::Build(ThreadContext& context, Node* node) {

            const size_t minLeafSize = size_t(settings.minLeafSize);
            // Create leaf node
            if (node->refs.size() <= minLeafSize) {
                // CreateLeaf(context);
                return;
            }

            auto nodeCost = float(node->refs.size()) * node->aabb.GetSurfaceArea();

            Split objectSplit;
            if (settings.quality == Quality::HIGH) {
                objectSplit = FindObjectSplit(context, node);
            }
            else {
                objectSplit = FindObjectSplitBinned(context, node);
            }

            Split spatialSplit;
            if (node->depth <= settings.maxSplitDepth && settings.quality != Quality::LOW) {
                AABB overlap = objectSplit.leftAABB;
                overlap.Intersect(objectSplit.rightAABB);
                if (overlap.GetSurfaceArea() >= minOverlap) {
                    spatialSplit = FindSpatialSplit(context, node);
                }
            }

            auto& leftRefs = context.leftRefs;
            auto& rightRefs = context.rightRefs;

            Split split;
            // If we haven't found a cost improvement we create a leaf node
            if ((objectSplit.axis < 0 || objectSplit.cost >= nodeCost) &&
                (spatialSplit.axis < 0 || spatialSplit.cost >= nodeCost)) {
                /*
                if (node->refs.size() >= 2 * minLeafSize) {
                    leftRefs.clear();
                    rightRefs.clear();
                    split = PerformMedianSplit(context, node, rightRefs, leftRefs);
                }
                else {
                    return;
                }
                */
                return;
            }
            else {
                // We need to keep this below this return statement, otherwise we'll get performance degradation
                leftRefs.clear();
                rightRefs.clear();
            }

            if (spatialSplit.cost < objectSplit.cost) {
                PerformSpatialSplit(context, node, rightRefs, leftRefs, spatialSplit);
                split = spatialSplit;
            }

            if (objectSplit.cost <= spatialSplit.cost) {
                leftRefs.clear();
                rightRefs.clear();
                if (settings.quality == Quality::HIGH) {
                    PerformObjectSplit(context, node, rightRefs, leftRefs, objectSplit);
                }
                else {
                    PerformObjectSplitBinned(context, node, rightRefs, leftRefs, objectSplit);
                }
                split = objectSplit;
            }

            node->refs.clear();
            node->refs.shrink_to_fit();

            if (node->depth <= 5) {
                std::thread leftBuilderThread, rightBuilderThread;

                auto leftLambda = [&]() {
                    if (!leftRefs.size()) return;
                    auto contextBinCount = settings.quality == HIGH ? uint32_t(2 * leftRefs.size()) : binCount;
                    auto leftContext = ThreadContext(contextBinCount, leftRefs.size());
                    auto leftNode = new Node(node->depth + 1, split.leftAABB, 2);
                    leftNode->refs = leftRefs;
                    node->children[0] = leftNode;
                    node->childrenBounds[0] = split.leftAABB;
                    Build(leftContext, leftNode);
                };

                auto rightLambda = [&]() {
                    if (!rightRefs.size()) return;
                    auto contextBinCount = settings.quality == HIGH ? uint32_t(2 * rightRefs.size()) : binCount;
                    auto rightContext = ThreadContext(contextBinCount, rightRefs.size());
                    auto rightNode = new Node(node->depth + 1, split.rightAABB, 2);
                    rightNode->refs = rightRefs;
                    node->children[1] = rightNode;
                    node->childrenBounds[1] = split.rightAABB;
                    Build(rightContext, rightNode);
                };

                leftBuilderThread = std::thread{ leftLambda };
                rightBuilderThread = std::thread{ rightLambda };

                leftBuilderThread.join();
                rightBuilderThread.join();
            }
            else {
                if (leftRefs.size()) {
                    auto leftNode = new Node(node->depth + 1, split.leftAABB, 2);
                    leftNode->refs = leftRefs;
                    node->children[0] = leftNode;
                    node->childrenBounds[0] = split.leftAABB;
                }

                if (rightRefs.size()) {
                    auto rightNode = new Node(node->depth + 1, split.rightAABB, 2);
                    rightNode->refs = rightRefs;
                    node->children[1] = rightNode;
                    node->childrenBounds[1] = split.rightAABB;
                }

                if (leftRefs.size()) Build(context, node->children[0]);
                if (rightRefs.size()) Build(context, node->children[1]);
            }

        }

        BVHBuilder::~BVHBuilder() {

            delete node;

        }

        BVHBuilder::Split BVHBuilder::FindObjectSplitBinned(ThreadContext& context, Node* node) {

            Split split;
            const auto depthBinCount = std::max(binCount / (node->depth + 1),
                uint32_t(settings.minDepthBinCount));

            auto& bins = context.bins;
            auto& rightAABBs = context.rightAABBs;

            // Iterate over 3 axises
            for (int32_t i = 0; i < 3; i++) {

                rightAABBs[0] = InitialAABB();
                rightAABBs[depthBinCount - 1] = InitialAABB();

                for (uint32_t i = 0; i < depthBinCount; i++) {
                    bins[i].aabb = InitialAABB();
                    bins[i].primitiveCount = 0;
                    rightAABBs[i] = InitialAABB();
                }

                auto start = node->aabb.min[i];
                auto stop = node->aabb.max[i];

                // If the dimension of this axis is to small continue
                if (fabsf(stop - start) < 1e-3f)
                    continue;

                auto binSize = (stop - start) / float(depthBinCount);
                auto invBinSize = 1.0f / binSize;

                for (const auto& ref : node->refs) {
                    const auto value = 0.5f * (ref.aabb.min[i] + ref.aabb.max[i]);

                    auto binIdx = uint32_t(glm::clamp((value - start) * invBinSize,
                        0.0f, float(depthBinCount) - 1.0f));

                    bins[binIdx].primitiveCount++;
                    bins[binIdx].aabb.Grow(ref.aabb);
                }

                // Sweep from right to left
                AABB rightAABB = InitialAABB();
                for (uint32_t j = depthBinCount - 1; j > 0; j--) {
                    rightAABB.Grow(bins[j].aabb);
                    rightAABBs[j - 1] = rightAABB;
                }

                AABB leftAABB = InitialAABB();
                uint32_t primitivesLeft = 0;

                // Sweep from left to right and attempt to
                // find cost improvement
                for (uint32_t j = 1; j < depthBinCount; j++) {
                    leftAABB.Grow(bins[j - 1].aabb);
                    primitivesLeft += bins[j - 1].primitiveCount;

                    const auto rightAABB = rightAABBs[j - 1];
                    const auto primitivesRight = uint32_t(node->refs.size()) - primitivesLeft;

                    if (!primitivesLeft || !primitivesRight) continue;

                    const auto leftSurface = leftAABB.GetSurfaceArea();
                    const auto rightSurface = rightAABB.GetSurfaceArea();

                    // Calculate cost for current split
                    const auto cost = 0.0f + (leftSurface * float(primitivesLeft) +
                        rightSurface * float(primitivesRight));

                    // Check if cost has improved
                    if (cost < split.cost) {
                        split.cost = cost;
                        split.axis = i;
                        split.binIdx = j;
                        split.pos = start + float(j) * binSize;

                        split.leftAABB = leftAABB;
                        split.rightAABB = rightAABB;
                    }
                }

            }

            return split;

        }

        BVHBuilder::Split BVHBuilder::FindObjectSplit(ThreadContext& context, Node* node) {

            Split split;

            auto depthBinCount = uint32_t(node->refs.size());

            auto& rightAABBs = context.rightAABBs;

            // Iterate over 3 axises
            for (int32_t i = 0; i < 3; i++) {

                rightAABBs[0] = InitialAABB();
                rightAABBs[depthBinCount - 1] = InitialAABB();

                auto start = node->aabb.min[i];
                auto stop = node->aabb.max[i];

                // If the dimension of this axis is to small continue
                if (fabsf(stop - start) < 1e-3f)
                    continue;

                std::sort(node->refs.begin(), node->refs.end(), [&](const Ref& ref0, const Ref& ref1) {
                    auto center0 = ref0.aabb.max[i] + ref0.aabb.min[i];
                    auto center1 = ref1.aabb.max[i] + ref1.aabb.min[i];
                    if (center0 != center1)
                        return center0 < center1;
                    else
                        return ref0.idx < ref1.idx;
                    });

                // Sweep from right to left
                AABB rightAABB = InitialAABB();
                for (uint32_t j = depthBinCount - 1; j > 0; j--) {
                    rightAABB.Grow(node->refs[j].aabb);
                    rightAABBs[j - 1] = rightAABB;
                }

                AABB leftAABB = InitialAABB();

                // Sweep from left to right and attempt to
                // find cost improvement
                for (uint32_t j = 1; j < depthBinCount; j++) {
                    auto& refAABB = node->refs[j - 1].aabb;
                    leftAABB.Grow(refAABB);

                    const auto rightAABB = rightAABBs[j - 1];
                    const auto primitivesLeft = j;
                    const auto primitivesRight = uint32_t(node->refs.size()) - primitivesLeft;

                    if (!primitivesLeft || !primitivesRight) continue;

                    const auto leftSurface = leftAABB.GetSurfaceArea();
                    const auto rightSurface = rightAABB.GetSurfaceArea();

                    // Calculate cost for current split
                    const auto cost = (leftSurface * float(primitivesLeft) +
                        rightSurface * float(primitivesRight));

                    auto center = refAABB.max[i] + refAABB.min[i];

                    // Check if cost has improved
                    if (cost < split.cost) {
                        split.cost = cost;
                        split.axis = i;
                        split.binIdx = j;
                        split.pos = center;

                        split.leftAABB = leftAABB;
                        split.rightAABB = rightAABB;
                    }
                }

            }

            return split;

        }

        void BVHBuilder::PerformObjectSplitBinned(ThreadContext& context, Node* node,
            std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs, Split& split) {

            const auto depthBinCount = std::max(binCount / (node->depth + 1),
                uint32_t(settings.minDepthBinCount));

            auto start = node->aabb.min[split.axis];
            auto stop = node->aabb.max[split.axis];

            auto binSize = (stop - start) / float(depthBinCount);
            auto invBinSize = 1.0f / binSize;

            for (const auto& ref : node->refs) {
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

        void BVHBuilder::PerformObjectSplit(ThreadContext& context, Node* node,
            std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs, Split& split) {

            std::sort(node->refs.begin(), node->refs.end(), [&](const Ref& ref0, const Ref& ref1) {
                auto center0 = ref0.aabb.max[split.axis] + ref0.aabb.min[split.axis];
                auto center1 = ref1.aabb.max[split.axis] + ref1.aabb.min[split.axis];
                if (center0 != center1)
                    return center0 < center1;
                else
                    return ref0.idx < ref1.idx;
                });

            for (int32_t i = 0; i < split.binIdx; i++) {
                leftRefs.push_back(node->refs[i]);
            }

            for (int32_t i = split.binIdx; i < int32_t(node->refs.size()); i++) {
                rightRefs.push_back(node->refs[i]);
            }

        }

        BVHBuilder::Split BVHBuilder::FindSpatialSplit(ThreadContext& context, Node* node) {

            Split split;
            const auto depthBinCount = std::max(binCount / (node->depth + 1),
                uint32_t(settings.minDepthBinCount));

            auto& bins = context.bins;
            auto& rightAABBs = context.rightAABBs;

            // Iterate over 3 axises
            for (int32_t i = 0; i < 3; i++) {

                rightAABBs[0] = InitialAABB();
                rightAABBs[depthBinCount - 1] = InitialAABB();

                for (uint32_t i = 0; i < depthBinCount; i++) {
                    bins[i].aabb = InitialAABB();
                    bins[i].primitiveCount = 0;
                    rightAABBs[i] = InitialAABB();
                }

                auto start = node->aabb.min[i];
                auto stop = node->aabb.max[i];

                // If the dimension of this axis is to small continue
                if (fabsf(stop - start) < 1e-3f)
                    continue;

                auto binSize = (stop - start) / float(depthBinCount);
                auto invBinSize = 1.0f / binSize;

                for (const auto& ref : node->refs) {
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
                for (uint32_t j = depthBinCount - 1; j > 0; j--) {
                    rightAABB.Grow(bins[j].aabb);
                    rightAABBs[j - 1] = rightAABB;
                }

                AABB leftAABB = InitialAABB();
                uint32_t primitivesRight = uint32_t(node->refs.size());
                uint32_t primitivesLeft = 0;

                // Sweep from left to right and attempt to
                // find cost improvement
                for (uint32_t j = 1; j < depthBinCount; j++) {
                    leftAABB.Grow(bins[j - 1].aabb);

                    primitivesLeft += bins[j - 1].enter;
                    primitivesRight -= bins[j - 1].exit;

                    const auto rightAABB = rightAABBs[j - 1];

                    if (!primitivesLeft || !primitivesRight) continue;

                    const auto leftSurface = leftAABB.GetSurfaceArea();
                    const auto rightSurface = rightAABB.GetSurfaceArea();

                    // Calculate cost for current split
                    const auto cost = 0.0f + (leftSurface * float(primitivesLeft) +
                        rightSurface * float(primitivesRight));

                    // Check if cost has improved
                    if (cost < split.cost) {
                        split.cost = cost;
                        split.axis = i;
                        split.binIdx = j;
                        split.pos = start + float(j) * binSize;

                        split.leftAABB = leftAABB;
                        split.rightAABB = rightAABB;
                    }
                }

            }

            return split;

        }

        void BVHBuilder::PerformSpatialSplit(ThreadContext& context, Node* node,
            std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs, Split& split) {

            const auto depthBinCount = std::max(binCount / (node->depth + 1),
                uint32_t(settings.minDepthBinCount));

            auto start = node->aabb.min[split.axis];
            auto stop = node->aabb.max[split.axis];

            auto binSize = (stop - start) / float(depthBinCount);
            auto invBinSize = 1.0f / binSize;

            split.leftAABB = InitialAABB();
            split.rightAABB = InitialAABB();

            for (const auto& ref : node->refs) {
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

            for (const auto& ref : node->refs) {
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

        BVHBuilder::Split BVHBuilder::PerformMedianSplit(ThreadContext& context,
            Node* node, std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs) {

            Split split;

            auto dimensions = node->aabb.max - node->aabb.min;
            auto axis = dimensions.x > dimensions.y ? dimensions.x > dimensions.z ? 0 : 2 :
                dimensions.y > dimensions.z ? 1 : 2;
            std::sort(node->refs.begin(), node->refs.end(), [&](const Ref& ref0, const Ref& ref1) {
                auto center0 = ref0.aabb.max[axis] + ref0.aabb.min[axis];
                auto center1 = ref1.aabb.max[axis] + ref1.aabb.min[axis];
                return center0 < center1;
                });

            auto splitIdx = uint32_t(node->refs.size() / 2);

            for (uint32_t i = 0; i < splitIdx; i++) {
                const auto& ref = node->refs[i];
                split.leftAABB.Grow(ref.aabb);
                leftRefs.push_back(ref);
            }

            for (uint32_t i = splitIdx; i < uint32_t(node->refs.size()); i++) {
                const auto& ref = node->refs[i];
                split.rightAABB.Grow(ref.aabb);
                rightRefs.push_back(ref);
            }

            return split;

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

        void BVHBuilder::CreateLeaf(ThreadContext& context) {



        }

        AABB BVHBuilder::InitialAABB() {

            const auto min = glm::vec3(std::numeric_limits<float>::max());
            const auto max = glm::vec3(-std::numeric_limits<float>::max());
            return AABB(min, max);

        }

        BVHBuilder::Node::Node(int32_t numChildren) : numChildren(numChildren) {

            children = new Node * [numChildren];
            childrenBounds.resize(numChildren);

        }

        BVHBuilder::Node::Node(int32_t depth, const AABB& aabb, int32_t numChildren)
            : numChildren(numChildren), depth(depth), aabb(aabb) {

            children = new Node * [numChildren];
            for (int32_t i = 0; i < numChildren; i++) children[i] = nullptr;
            childrenBounds.resize(numChildren);

        }

        void BVHBuilder::Node::Flatten(std::vector<BVH2Node>& nodes, std::vector<BVHBuilder::Ref>& refs) {

            // Check for leaf
            if (this->refs.size()) {
                refs.insert(refs.end(), this->refs.begin(), this->refs.end());
                refs[refs.size() - 1].endOfNode = true;
            }
            else {
                auto leftChild = children[0];
                auto rightChild = children[1];

                const auto nodeIdx = nodes.size();
                nodes.push_back(BVH2Node());

                // Flatten recursively
                if (leftChild && (leftChild->children || leftChild->refs.size())) {
                    auto leaf = leftChild->refs.size() > 0;
                    nodes[nodeIdx].leftAABB = childrenBounds[0];
                    nodes[nodeIdx].leftPtr = leaf ? ~int32_t(refs.size()) : uint32_t(nodes.size());
                    leftChild->Flatten(nodes, refs);
                }
                if (rightChild && (rightChild->children || rightChild->refs.size())) {
                    auto leaf = rightChild->refs.size() > 0;
                    nodes[nodeIdx].rightAABB = childrenBounds[1];
                    nodes[nodeIdx].rightPtr = leaf ? ~int32_t(refs.size()) : uint32_t(nodes.size());
                    rightChild->Flatten(nodes, refs);
                }
            }

        }

        void BVHBuilder::Node::Flatten(std::vector<BVH4Node>& nodes, std::vector<BVHBuilder::Ref>& refs) {

            // Check for leaf
            if (this->refs.size()) {
                refs.insert(refs.end(), this->refs.begin(), this->refs.end());
                refs[refs.size() - 1].endOfNode = true;
            }
            else {
                const auto nodeIdx = nodes.size();
                nodes.push_back(BVH4Node());

                for (int32_t i = 0; i < 4; i++) {
                    if (i < numChildren) {
                        auto leaf = children[i]->refs.size() > 0;

                        nodes[nodeIdx].childrenBounds[i] = childrenBounds[i];
                        nodes[nodeIdx].childrenPtr[i] = leaf ? ~int32_t(refs.size()) : int32_t(nodes.size());
                        children[i]->Flatten(nodes, refs);
                    }
                    else {
                        nodes[nodeIdx].childrenBounds[i] = AABB(vec3(10.0f), vec3(-10.0f));
                        nodes[nodeIdx].childrenPtr[i] = 0;
                    }
                }
            }

        }

        void BVHBuilder::Node::Collapse(int32_t maxChildren) {

            if (refs.size() > 0) return;

            auto newChildren = new Node * [maxChildren];
            childrenBounds.resize(maxChildren);

            for (int32_t i = 0; i < maxChildren; i++) {
                newChildren[i] = i < numChildren ? children[i] : nullptr;
            }

            delete[] children;
            children = newChildren;

            while (true) {
                float maxSurfaceArea = 0.0f;
                int32_t maxIdx = -1;
                for (int32_t i = 0; i < numChildren; i++) {
                    auto child = children[i];
                    auto& childBounds = childrenBounds[i];
                    auto childValid = child && child->childrenBounds.size() && !child->refs.size();
                    if (childValid && child->numChildren + numChildren - 1 <= maxChildren) {
                        auto surfaceArea = childBounds.GetSurfaceArea();
                        if (surfaceArea > maxSurfaceArea) {
                            maxIdx = i;
                            maxSurfaceArea = surfaceArea;
                        }
                    }
                }

                if (maxIdx < 0) break;

                // Decrement and swap child with last child to prevent overwriting valid data
                std::swap(children[maxIdx], children[--numChildren]);
                std::swap(childrenBounds[maxIdx], childrenBounds[numChildren]);
                auto child = children[numChildren];

                for (int32_t i = 0; i < child->numChildren; i++) {
                    // Embree is sometimes really weird
                    if (!child->children[i]) continue;
                    auto childIdx = numChildren++;
                    children[childIdx] = child->children[i];
                    childrenBounds[childIdx] = child->childrenBounds[i];
                }

                delete[] child->children;
                delete child;
            }

            for (int32_t i = 0; i < numChildren; i++)
                children[i]->Collapse(maxChildren);

        }

        void BVHBuilder::Node::Clear() {

            for (int32_t i = 0; i < numChildren; i++) {
                if (children[i]) {
                    children[i]->Clear();
                    // These were all single allocations
                    delete children[i];
                }
            }

            delete[] children;

        }

    }

}