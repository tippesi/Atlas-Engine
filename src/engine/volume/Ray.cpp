#include "Ray.h"

namespace Atlas {

    namespace Volume {

        Ray::Ray(vec3 origin, vec3 direction, float tMin, float tMax) : origin(origin),
            direction(direction), inverseDirection(1.0f / direction), tMin(tMin), tMax(tMax) {



        }

        vec3 Ray::Get(float distance) const {

            return origin + distance * direction;

        }

        bool Ray::Intersects(const AABB& aabb) {

            auto t = 0.0f;

            return Intersects(aabb, t);

        }

        bool Ray::Intersects(const AABB& aabb, float& t) {

            auto t0 = (aabb.min - origin) * inverseDirection;
            auto t1 = (aabb.max - origin) * inverseDirection;

            auto tsmall = glm::min(t0, t1);
            auto tbig = glm::max(t0, t1);

            auto tminf = glm::max(tMin, glm::max(tsmall.x, glm::max(tsmall.y, tsmall.z)));
            auto tmaxf = glm::min(tMax, glm::min(tbig.x, glm::min(tbig.y, tbig.z)));

            t = tminf;

            return (tminf <= tmaxf);

        }

        bool Ray::Intersects(vec3 v0, vec3 v1, vec3 v2) {

            vec3 intersection;

            return Intersects(v0, v1, v2, intersection);

        }

        bool Ray::Intersects(vec3 v0, vec3 v1, vec3 v2, vec3& intersection) {

            auto e0 = v1 - v0;
            auto e1 = v2 - v0;
            auto s = origin - v0;

            auto p = cross(s, e0);
            auto q = cross(direction, e1);
            
            auto sol = vec3(glm::dot(p, e1), glm::dot(q, s),
                glm::dot(p, direction)) / (glm::dot(q, e0));

            if (sol.x >= 0.0f && sol.y >= 0.0f && 
                sol.z >= 0.0f && sol.y + sol.z <= 1.0f) {

                intersection = sol;

                return true;

            }

            return false;

        }

        bool Ray::Intersects(const Rectangle& rect, float& t) {

            auto N = rect.GetNormal();

            auto nDotD = glm::dot(N, direction);
            if (nDotD >= 0.0f)
                return false;

            float dist = glm::dot(rect.point - origin, N) / nDotD;

            if (dist <= tMin && dist > tMax)
                return false;

            auto point = Get(dist) - rect.point;

            auto projS0 = glm::dot(rect.s0, point);
            auto projS1 = glm::dot(rect.s1, point);

            if (projS0 >= 0.0f && projS0 <= glm::dot(rect.s0, rect.s0) &&
                projS1 >= 0.0f && projS1 <= glm::dot(rect.s1, rect.s1)) {
                t = dist;
                return true;
            }

            return false;

        }

        vec3 Ray::Distance(Ray ray, float& distance) {

            constexpr float minFloat = std::numeric_limits<float>::min();

            auto cross = glm::cross(direction, ray.direction);
            auto denom = glm::max(glm::pow(glm::length(cross), 2.0f),
                minFloat);

            auto t = ray.origin - origin;
            auto det0 = glm::determinant(mat3(t, ray.direction, cross));
            auto det1 = glm::determinant(mat3(t, direction, cross));
            
            auto t1 = det1 / denom;

            distance = t1;

            return ray.Get(t1);

        }

    }

}