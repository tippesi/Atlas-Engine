#include "Ray.h"

namespace Atlas {

	namespace Common {

		Ray::Ray(vec3 origin, vec3 direction) : origin(origin), direction(direction) {

			inverseDirection = 1.0f / direction;

		}

		bool Ray::Intersects(AABB aabb, float tmin, float tmax) {

			auto t0 = (aabb.min - origin) * inverseDirection;
			auto t1 = (aabb.max - origin) * inverseDirection;

			auto tsmall = glm::min(t0, t1);
			auto tbig = glm::max(t0, t1);

			auto tminf = glm::max(tmin, glm::max(tsmall.x, glm::max(tsmall.y, tsmall.z)));
			auto tmaxf = glm::min(tmax, glm::min(tbig.x, glm::min(tbig.y, tbig.z)));

			return (tminf < tmaxf);

		}

		bool Ray::Intersects(vec3 v0, vec3 v1, vec3 v2) {

			vec3 intersection;
			float t;

			return Intersects(v0, v1, v2, t, intersection);

		}

		bool Ray::Intersects(vec3 v0, vec3 v1, vec3 v2, float& t, vec3& intersection) {

			auto e0 = v1 - v0;
			auto e1 = v2 - v0;
			auto s = origin - v0;

			auto p = cross(s, e0);
			auto q = cross(direction, e1);
			
			auto sol = vec3(glm::dot(p, e1), glm::dot(q, s),
				glm::dot(p, direction)) / (glm::dot(q, e0));

			if (sol.x >= 0.0f && sol.y >= 0.0f && 
				sol.z >= 0.0f && sol.y + sol.z <= 1.0f) {

				intersection = v0 + e0 * sol.y + e1 * sol.z;
				t = sol.x;

				return true;

			}

			return false;

		}

	}

}