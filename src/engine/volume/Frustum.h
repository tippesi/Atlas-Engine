#ifndef AE_FRUSTUM_H
#define AE_FRUSTUM_H

#include "../System.h"

#include "AABB.h"

#include <vector>

namespace Atlas {

    namespace Volume {

        class Frustum {

        public:
            /**
             * Constructs a Frustum object.
             */
            Frustum() = default;

            /**
             * Constructs a Frustum object.
             * @param corners The 8 corners of the frustum.
             * @note The corners must be in the following order
             * with the far plane corners first and the near plane corners second:
             * Far plane: Upper left, upper right, bottom left, bottom right
             * Near plane: Upper left, upper right, bottom left, bottom right
             */
            explicit Frustum(const std::vector<vec3>& corners);

            /**
             * Constructs a Frustum object.
             * @param matrix The view projection matrix of the frustum.
             */
            explicit Frustum(const mat4& matrix);

            /**
             * Resizes the frustum.
             * @param corners The 8 corners of the frustum.
             * @note The corners must be in the following order
             * with the far plane corners first and the near plane corners second:
             * Far plane: Upper left, upper right, bottom left, bottom right
             * Near plane: Upper left, upper right, bottom left, bottom right
             */
            void Resize(const std::vector<vec3>& corners);

            /**
             * Resizes the frustum.
             * @param matrix The view projection matrix of the frustum.
             */
            void Resize(const mat4& matrix);

            /**
             * Checks if the AABB intersects the frustum
             * @param aabb An AABB to test against the frustum
             * @return True if visible, false otherwise.
             */
            bool Intersects(const AABB& aabb) const;

            /**
             * Checks if the AABB is inside the frustum
             * @param aabb An AABB to test against the frustum
             * @return True if visible, false otherwise.
             */
            bool IsInside(const AABB& aabb) const;

            /**
             * Returns the planes of the frustum as 4-component vectors
             * @return The planes of the frustum
             * @note The xyz components encode the normal, the w component
             * encodes the distance -dot(normal, planeOrigin)
             */
            std::vector<vec4> GetPlanes() const;

            /**
            * Returns the corners of the frustum.
            * @retrun  The 8 corners of the frustum.
            * @note The corners will be in the following order
            * with the far plane corners first and the near plane corners second:
            * Far plane: Upper left, upper right, bottom left, bottom right
            * Near plane: Upper left, upper right, bottom left, bottom right
            */
            std::vector<vec3> GetCorners() const;

        private:
            void CalculateCorners(const mat4& matrix);

            enum {
                NEAR_PLANE = 0,    FAR_PLANE, TOP_PLANE,
                BOTTOM_PLANE, LEFT_PLANE, RIGHT_PLANE
            };

            struct Plane {
                Plane() {}

                Plane(vec3 v0, vec3 v1, vec3 v2) {
                    auto d0 = v0 - v1;
                    auto d1 = v2 - v1;
                    normal = glm::normalize(glm::cross(d1, d0));
                    distance = -glm::dot(normal, v1);
                }

                vec3 normal = vec3(0.0f);
                float distance = 0.0f;
            };

            std::vector<vec3> corners;
            Plane planes[6];

        };

    }

}

#endif