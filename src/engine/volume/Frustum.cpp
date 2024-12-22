#include "Frustum.h"

namespace Atlas {

    namespace Volume {

        Frustum::Frustum(const std::array<vec3, 8>& corners) {

            Resize(corners);

        }

        Frustum::Frustum(const mat4& matrix) {

            Resize(matrix);

        }

        void Frustum::Resize(const std::array<vec3, 8>& corners) {

            this->corners = corners;
            planes[NEAR_PLANE] = Plane(corners[0], corners[1], corners[2]);
            planes[FAR_PLANE] = Plane(corners[5], corners[4], corners[7]);
            planes[TOP_PLANE] = Plane(corners[4], corners[0], corners[6]);
            planes[BOTTOM_PLANE] = Plane(corners[1], corners[5], corners[3]);
            planes[RIGHT_PLANE] = Plane(corners[4], corners[5], corners[0]);
            planes[LEFT_PLANE] = Plane(corners[2], corners[3], corners[6]);

        }

        void Frustum::Resize(const mat4& matrix) {

            CalculateCorners(matrix);
            Resize(corners);

        }

        bool Frustum::Intersects(const AABB& aabb) const {

            for (uint8_t i = 0; i < 6; i++) {

                auto& normal = planes[i].normal;
                auto& distance = planes[i].distance;

                vec3 s;
                s.x = normal.x >= 0.0f ? aabb.max.x : aabb.min.x;
                s.y = normal.y >= 0.0f ? aabb.max.y : aabb.min.y;
                s.z = normal.z >= 0.0f ? aabb.max.z : aabb.min.z;

                if (distance + glm::dot(normal, s) < 0.0f)
                    return false;

            }

            return true;

        }

        bool Frustum::IsInside(const AABB& aabb) const {

            // Iterate through each frustum plane
            for (const Plane& plane : planes) {
                // Test all 8 corners of the AABB
                bool allInside = true;
                for (int i = 0; i < 8; i++) {
                    glm::vec3 corner = glm::vec3(
                        (i & 1) ? aabb.max.x : aabb.min.x,
                        (i & 2) ? aabb.max.y : aabb.min.y,
                        (i & 4) ? aabb.max.z : aabb.min.z
                    );

                    if (plane.distance + glm::dot(plane.normal, corner) < 0.0f)
                        return false;
                }
            }
            return true;

        }

        std::vector<vec4> Frustum::GetPlanes() const {

            std::vector<vec4> planes;
            planes.reserve(6);

            for (uint8_t i = 0; i < 6; i++) {
                planes.push_back(vec4(this->planes[i].normal,
                    this->planes[i].distance));
            }

            return planes;

        }

        std::array<vec3, 8> Frustum::GetCorners() const {

            return corners;

        }

        void Frustum::CalculateCorners(const mat4& matrix) {

            const mat4 clipMatrix = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.0f, 0.0f, 0.5f, 1.0f);

            // Somehow far and near points are reversed
            vec3 vectors[8] = {
                vec3(-1.0f, 1.0f, 0.0f),
                vec3(1.0f, 1.0f, 0.0f),
                vec3(-1.0f, -1.0f, 0.0f),
                vec3(1.0f, -1.0f, 0.0f),
                vec3(-1.0f, 1.0f, 1.0f),
                vec3(1.0f, 1.0f, 1.0f),
                vec3(-1.0f, -1.0f, 1.0f),
                vec3(1.0f, -1.0f, 1.0f)
            };

            auto inverseMatrix = glm::inverse(glm::inverse(clipMatrix) * matrix);

            for (uint8_t i = 0; i < 8; i++) {
                auto homogenous = inverseMatrix * vec4(vectors[i], 1.0f);
                corners[i] = vec3(homogenous) / homogenous.w;
            }

        }

    }

}