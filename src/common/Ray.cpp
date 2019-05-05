#include "Ray.h"

namespace Atlas {

	namespace Common {

		bool Ray::Intersects(AABB aabb) {

			vec3 inverseDirection = 1.0f / direction;



			return true;

		}

		bool Ray::Intersects(vec3 point0, vec3 point1, vec3 point2) {

			vec3 intersection;

			return Intersects(point0, point1, point2, intersection);

		}

		bool Ray::Intersects(vec3 point0, vec3 point1, vec3 point2, vec3& intersection) {

			auto e0 = point1 - point0;
			auto e1 = point2 - point0;
			auto s = origin - point0;
			
			auto sol = vec3(glm::dot(glm::cross(s, e0), e1),
				glm::dot(glm::cross(direction, e1), s),
				glm::dot(glm::cross(s, e1), direction)) / 
				(glm::dot(glm::cross(direction, e1), e0));

			if (sol.x >= 0.0f && sol.y >= 0.0f && 
				sol.z >= 0.0f && sol.y + sol.z <= 1.0f) {

				intersection = point0 + e0 * sol.y + e1 * sol.z;

				return true;

			}

			return false;

		}

	}

}