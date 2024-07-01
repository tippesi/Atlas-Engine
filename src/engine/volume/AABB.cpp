#include "AABB.h"

namespace Atlas {

    namespace Volume {

        AABB::AABB(vec3 min, vec3 max) : min(min), max(max) {



        }

        bool AABB::Intersects(AABB aabb) {

            return aabb.min.x <= max.x && aabb.max.x >= min.x &&
                aabb.min.y <= max.y && aabb.max.y >= min.y &&
                aabb.min.z <= max.z && aabb.max.z >= min.z;

        }

        bool AABB::IsInside(vec3 point) {

            return point.x >= min.x && point.x <= max.x &&
                point.y >= min.y && point.y <= max.y &&
                point.z >= min.z && point.z <= max.z;

        }

        bool AABB::IsInside(AABB aabb) {

            return IsInside(aabb.min) && IsInside(aabb.max);

        }

        AABB AABB::Transform(mat4 matrix) {

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

        AABB AABB::Translate(vec3 translation) {

            return AABB(min + translation, max + translation);

        }

        AABB AABB::Scale(float scale) {

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