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

        class WoopTriangle {
        public:
            WoopTriangle() = default;

            WoopTriangle(BVHTriangle& triangle) {
                auto m0 = triangle.v1 - triangle.v0;
                auto m1 = triangle.v2 - triangle.v0;

                N = glm::normalize(glm::cross(m0, m1));

                auto m2 = N - triangle.v0;
                auto mat = glm::mat4(
                    glm::vec4(m0, 0.0f),
                    glm::vec4(m1, 0.0f),
                    glm::vec4(m2, 0.0f),
                    glm::vec4(triangle.v0, 1.0f));
                mat = glm::inverse(mat);

                m = glm::mat3(mat);
                N = glm::vec3(mat[3]);
            }

            inline bool Intersect(Ray& ray, glm::vec3& intersection, float tmax) {

                auto o = ray.origin;
                auto d = ray.direction;

                auto m2 = glm::vec4(m[0][2], m[1][2], m[2][2], 1.0f);
                auto oz = glm::dot(m2, glm::vec4(o, N.z));
                auto dz = glm::dot(glm::vec3(m2), d);
                auto t = -oz / dz;
                if (t >= 0.0f && t <= tmax) {
                    auto m0 = glm::vec4(m[0][0], m[1][0], m[2][0], 1.0f);
                    auto ox = glm::dot(m0, glm::vec4(o, N.x));
                    auto dx = glm::dot(glm::vec3(m0), d);

                    auto u = ox + t * dx;
                    if (u >= 0.0f && u <= 1.0f) {
                        auto m1 = glm::vec4(m[0][1], m[1][1], m[2][1], 1.0f);
                        auto oy = glm::dot(m1, glm::vec4(o, N.y));
                        auto dy = glm::dot(glm::vec3(m1), d);

                        auto v = oy + t * dy;
                        if (v >= 0.0f && u + v <= 1.0f) {
                            intersection = glm::vec3(t, u, v);
                            return true;
                        }
                    }
                }

                return false;
            }

            glm::mat3 m;
            glm::vec3 N;
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
            std::vector<WoopTriangle> woopData;

            std::vector<BVHNode> nodes;

        };

    }

}

#endif