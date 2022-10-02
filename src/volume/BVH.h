#ifndef AE_BVH_H
#define AE_BVH_H

#include "AABB.h"
#include "Ray.h"

#include <vector>
#include <algorithm>

namespace Atlas {

    namespace Volume {

        class BVH2Node {
        public:
            BVH2Node() {}

            AABB leftAABB;
            AABB rightAABB;

            int32_t leftPtr = 0;
            int32_t rightPtr = 0;

        };

        class BVH4Node {
        public:
            BVH4Node() {}

            AABB childrenBounds[4];
            int32_t childrenPtr[4];
        };

        class BVHTriangle {
        public:
            vec3 v0;
            vec3 v1;
            vec3 v2;
            uint32_t idx;
            bool endOfNode = false;
        };

        class BVHBuilder {

        public:
            struct Bin {
            public:
                AABB aabb = InitialAABB();
                uint32_t primitiveCount = 0;

                uint32_t enter = 0;
                uint32_t exit = 0;
            };

            struct Ref {
                uint32_t idx = 0;
                bool endOfNode = false;
                AABB aabb = InitialAABB();
            };

            enum Quality {
                LOW = 0,
                MEDIUM,
                HIGH
            };

            struct Settings {
                int32_t quality = Quality::MEDIUM;
                int32_t binCount = 128;
                int32_t minDepthBinCount = 16;
                int32_t maxSplitDepth = 16;
                int32_t minLeafSize = 8;
                float minOverlap = 10e-6f;
            };

            class ThreadContext {

            public:
                ThreadContext(const uint32_t binCount, const size_t refEstimate) {
                    bins.resize(binCount);
                    rightAABBs.resize(binCount);

                    leftRefs.reserve(refEstimate / 2);
                    rightRefs.reserve(refEstimate / 2);
                }

                std::vector<Bin> bins;
                std::vector<AABB> rightAABBs;

                std::vector<Ref> leftRefs;
                std::vector<Ref> rightRefs;

            };

            class Node {
            public:
                Node() = default;

                Node(int32_t numChildren);

                Node(const int32_t depth, const AABB& aabb, int32_t numChildren);

                void Flatten(std::vector<BVH2Node>& nodes, std::vector<BVHBuilder::Ref>& refs);

                void Flatten(std::vector<BVH4Node>& nodes, std::vector<BVHBuilder::Ref>& refs);

                void Collapse(int32_t maxChildren = 4);

                void Clear();

                Node** children = nullptr;
                std::vector<AABB> childrenBounds;
                int32_t numChildren = 0;
                int32_t depth = 0;

                AABB aabb;

                std::vector<BVHBuilder::Ref> refs;

            private:
                void ClearOwnMemory();

            };

            BVHBuilder(const BVHTriangle* data, size_t dataCount);

            ~BVHBuilder();

            void Build();

            void Build(ThreadContext& context, Node* node);

            Node* node = nullptr;
            
            Settings settings;

        private:
            struct Split {
                float cost = std::numeric_limits<float>::max();
                int32_t axis = -1;
                uint32_t binIdx = 0;
                float pos = 0.0f;

                AABB leftAABB = InitialAABB();
                AABB rightAABB = InitialAABB();
            };

            Split FindObjectSplitBinned(ThreadContext& context, Node* node);

            Split FindObjectSplit(ThreadContext& context, Node* node);

            void PerformObjectSplitBinned(ThreadContext& context, Node* node,
                std::vector<Ref>& rightRefs,
                std::vector<Ref>& leftRefs, Split& split);

            void PerformObjectSplit(ThreadContext& context, Node* node,
                std::vector<Ref>& rightRefs,
                std::vector<Ref>& leftRefs, Split& split);

            Split FindSpatialSplit(ThreadContext& context, Node* node);

            void PerformSpatialSplit(ThreadContext& context, Node* node,
                std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs, Split& split);

            Split PerformMedianSplit(ThreadContext& context, Node* node,
                std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs);

            void SplitReference(const BVHTriangle triangle, Ref currentRef,
                Ref& leftRef, Ref& rightRef, const float planePos, const int32_t axis);

            void CreateLeaf(ThreadContext& context);

            static AABB InitialAABB();

            const BVHTriangle* data;

            uint32_t binCount = 128;
            float minOverlap = 0.0f;

        };

    }

}

#endif