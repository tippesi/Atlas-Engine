#ifndef AE_BVH_H
#define AE_BVH_H

#include "AABB.h"
#include "Ray.h"

#include <vector>
#include <algorithm>

namespace Atlas {

    namespace Volume {

        class BVHNode {
        public:
            BVHNode() {}

            AABB leftAABB;
            AABB rightAABB;

            int32_t leftPtr = 0;
            int32_t rightPtr = 0;

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
            struct Ref {
                uint32_t idx = 0;
                bool endOfNode = false;
                AABB aabb = InitialAABB();
            };

            BVHBuilder(const AABB aabb, const std::vector<Ref> refs,
                uint32_t depth, const float minOverlap, const uint32_t binCount = 128);

            ~BVHBuilder();

            void Build(const std::vector<BVHTriangle>& data);

            void Flatten(std::vector<BVHNode>& nodes, std::vector<Ref>& refs);

            BVHBuilder* leftChild = nullptr;
            BVHBuilder* rightChild = nullptr;

            uint32_t depth;
            uint32_t binCount;

            float minOverlap;
            float nodeCost;

            AABB aabb;
            std::vector<Ref> refs;

            static uint32_t maxDepth;
            static uint32_t minTriangles;
            static uint32_t maxTriangles;
            static uint32_t spatialSplitCount;
            static float totalSurfaceArea;

        private:
            struct Bin {
            public:
                AABB aabb = InitialAABB();
                uint32_t primitiveCount = 0;

                uint32_t enter = 0;
                uint32_t exit = 0;
            };

            struct Split {
                float cost = std::numeric_limits<float>::max();
                int32_t axis = -1;
                uint32_t binIdx = 0;
                float pos = 0.0f;

                AABB leftAABB = InitialAABB();
                AABB rightAABB = InitialAABB();
            };

            Split FindObjectSplit();

            void PerformObjectSplit(std::vector<Ref>& rightRefs,
                std::vector<Ref>& leftRefs, Split& split);

            Split FindSpatialSplit(const std::vector<BVHTriangle>& data);

            void PerformSpatialSplit(const std::vector<BVHTriangle>& data,
                std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs, Split& split);

            void SplitReference(const BVHTriangle triangle, Ref currentRef,
                Ref& leftRef, Ref& rightRef, const float planePos, const int32_t axis);

            Split PerformMedianSplit(std::vector<Ref>& rightRefs, std::vector<Ref>& leftRefs);

            void CreateLeaf();

            static AABB InitialAABB();

        };


        class BVH {

        public:
            BVH() = default;

            BVH(std::vector<AABB>& aabbs, std::vector<BVHTriangle>& data);

            bool GetIntersection(std::vector<std::pair<int32_t, float>>& stack, Ray ray, BVHTriangle& closest,
                glm::vec3& intersection);

            bool GetIntersection(std::vector<std::pair<int32_t, float>>& stack, Ray ray, BVHTriangle& closest,
                glm::vec3& intersection, float max);

            bool GetIntersectionAny(std::vector<std::pair<int32_t, float>>& stack, Ray ray, float max);

            std::vector<BVHNode>& GetTree();

            void Clear();

            std::vector<AABB> aabbs;
            std::vector<BVHTriangle> data;

            std::vector<BVHNode> nodes;

        };

    }

}

#endif