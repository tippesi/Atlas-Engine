#include "Ray.h"

namespace Atlas {

	namespace Volume {

		Ray::Ray(vec3 origin, vec3 direction) : origin(origin), direction(direction) {

			inverseDirection = 1.0f / direction;

		}

		bool Ray::Intersects(AABB aabb, float tmin, float tmax) {

			auto t = 0.0f;

			return Intersects(aabb, tmin, tmax, t);

		}

		bool Ray::Intersects(AABB aabb, float tmin, float tmax, float& t) {

			auto t0 = (aabb.min - origin) * inverseDirection;
			auto t1 = (aabb.max - origin) * inverseDirection;

			auto tsmall = glm::min(t0, t1);
			auto tbig = glm::max(t0, t1);

			auto tminf = glm::max(tmin, glm::max(tsmall.x, glm::max(tsmall.y, tsmall.z)));
			auto tmaxf = glm::min(tmax, glm::min(tbig.x, glm::min(tbig.y, tbig.z)));

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

	}

}