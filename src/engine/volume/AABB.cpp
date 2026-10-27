#include "AABB.h"

#include <algorithm>

namespace Atlas {

    namespace Volume {

        AABB::AABB(vec3 min, vec3 max) : min(min), max(max) {



        }

        bool AABB::Intersects(AABB aabb) const {

            return aabb.min.x <= max.x && aabb.max.x >= min.x &&
                aabb.min.y <= max.y && aabb.max.y >= min.y &&
                aabb.min.z <= max.z && aabb.max.z >= min.z;

        }

        bool AABB::IntersectsTriangle(vec3 v0, vec3 v1, vec3 v2) const {

            // Separating axis test with the box faces, the triangle normal and the edge cross products
            auto center = GetCenter();
            auto halfSize = GetSize() * 0.5f;
            v0 -= center;
            v1 -= center;
            v2 -= center;
            vec3 edges[3] = { v1 - v0, v2 - v1, v0 - v2 };

            auto separated = [&](vec3 axis) {
                if (glm::dot(axis, axis) < 1e-12f)
                    return false;
                auto p0 = glm::dot(v0, axis), p1 = glm::dot(v1, axis), p2 = glm::dot(v2, axis);
                auto radius = glm::dot(halfSize, glm::abs(axis));
                return std::min({ p0, p1, p2 }) > radius || std::max({ p0, p1, p2 }) < -radius;
            };

            for (int32_t i = 0; i < 3; i++) {
                vec3 boxAxis(0.0f);
                boxAxis[i] = 1.0f;
                if (separated(boxAxis))
                    return false;
                for (const auto& edge : edges)
                    if (separated(glm::cross(boxAxis, edge)))
                        return false;
            }

            return !separated(glm::cross(edges[0], edges[1]));

        }

        bool AABB::IsInside(vec3 point) const {

            return point.x >= min.x && point.x <= max.x &&
                point.y >= min.y && point.y <= max.y &&
                point.z >= min.z && point.z <= max.z;

        }

        bool AABB::IsInside(AABB aabb) const {

            return IsInside(aabb.min) && IsInside(aabb.max);

        }

        AABB AABB::Transform(mat4 matrix) const {

            vec3 cube[] = { vec3(min.x, min.y, min.z), vec3(min.x, min.y, max.z),
                vec3(max.x, min.y, min.z), vec3(max.x, min.y, max.z),
                vec3(min.x, max.y, min.z), vec3(min.x, max.y, max.z),
                vec3(max.x, max.y, min.z), vec3(max.x, max.y, max.z) };

            for (uint8_t i = 0; i < 8; i++) {
                cube[i] = matrix * vec4(cube[i], 1.0f);
            }

            vec3 newMin = cube[0], newMax = cube[0];

            for (uint8_t i = 1; i < 8; i++) {
                newMin.x = glm::min(newMin.x, cube[i].x);
                newMin.y = glm::min(newMin.y, cube[i].y);
                newMin.z = glm::min(newMin.z, cube[i].z);

                newMax.x = glm::max(newMax.x, cube[i].x);
                newMax.y = glm::max(newMax.y, cube[i].y);
                newMax.z = glm::max(newMax.z, cube[i].z);
            }

            return AABB(newMin, newMax);

        }

        AABB AABB::Translate(vec3 translation) const {

            return AABB(min + translation, max + translation);

        }

        AABB AABB::Scale(float scale) const {

            auto center = 0.5f * (min + max);
            auto scaledMin = center + scale * (min - center);
            auto scaledMax = center + scale * (max - center);

            return AABB(scaledMin, scaledMax);

        }

        AABB AABB::Scale(vec3 scale) const {

            auto center = 0.5f * (min + max);
            auto scaledMin = center + scale * (min - center);
            auto scaledMax = center + scale * (max - center);

            return AABB(scaledMin, scaledMax);

        }

        void AABB::Grow(AABB aabb) {

            max = glm::max(max, aabb.max);
            min = glm::min(min, aabb.min);

        }

        void AABB::Grow(glm::vec3 vector) {

            max = glm::max(vector, max);
            min = glm::min(vector, min);

        }

        void AABB::Intersect(AABB aabb) {

            min = glm::max(min, aabb.min);
            max = glm::min(max, aabb.max);

        }

        float AABB::GetSurfaceArea() const {
            auto dimension = max - min;

            return 2.0f * (dimension.x * dimension.y +
                dimension.y * dimension.z +
                dimension.z * dimension.x);
        }

        vec3 AABB::GetSize() const {

            return max - min;

        }

        float AABB::GetDistance(vec3 point) const {

            auto closestPoint = glm::clamp(point, min, max);

            return glm::distance(point, closestPoint);

        }

        vec3 AABB::GetCenter() const {

            return 0.5f * (max + min);

        }

        std::vector<vec3> AABB::GetCorners() {

            std::vector<vec3> corners;

            vec3 cube[] = { vec3(min.x, min.y, max.z), vec3(max.x, min.y, max.z),
                vec3(max.x, max.y, max.z), vec3(min.x, max.y, max.z),
                vec3(min.x, min.y, min.z), vec3(max.x, min.y, min.z),
                vec3(max.x, max.y, min.z), vec3(min.x, max.y, min.z) };

            for (uint8_t i = 0; i < 8; i++)
                corners.push_back(cube[i]);

            return corners;

        }

    }

}