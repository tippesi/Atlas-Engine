#ifndef AE_RAY_H
#define AE_RAY_H

#include "../System.h"
#include "AABB.h"
#include "Rectangle.h"

namespace Atlas {

    namespace Volume {

        template<class T>
        struct RayResult {
            bool IsNormalValid() const { return normal.x != 0.0f || normal.y != 0.0f || normal.z != 0.0f; }

            bool valid = false;
            vec3 normal = vec3(0.0f);
            float hitDistance = 10e12f;
            T data;
        };

        class Ray {

        public:
            Ray() = default;

            Ray(vec3 origin, vec3 direction, float tMin = 0.0f, float tMax = 2048.0f);

            vec3 Get(float distance) const;

            bool Intersects(const AABB& aabb);

            bool Intersects(const AABB& aabb, float& t);

            bool Intersects(vec3 v0, vec3 v1, vec3 v2);

            bool Intersects(vec3 v0, vec3 v1, vec3 v2, vec3& intersection);

            bool Intersects(const Rectangle& rect, float& t);

            vec3 Distance(Ray ray, float& distance);

            vec3 origin = vec3(0.0f);
            vec3 direction = vec3(0.0f, 1.0f, 0.0f);

            float tMin = 0.0f;
            float tMax = 2048.0f;

        private:
            vec3 inverseDirection = vec3(0.0f, 1.0f, 0.0f);

        };

    }

}


#endif