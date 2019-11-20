#include "Frustum.h"

namespace Atlas {

	namespace Volume {

		Frustum::Frustum(std::vector<vec3> corners) {

			Resize(corners);

		}

		void Frustum::Resize(std::vector<vec3> corners) {

			planes[NEAR_PLANE] = Plane(corners[4], corners[5], corners[7]);
			planes[FAR_PLANE] = Plane(corners[1], corners[0], corners[2]);
			planes[TOP_PLANE] = Plane(corners[5], corners[4], corners[0]);
			planes[BOTTOM_PLANE] = Plane(corners[6], corners[7], corners[3]);
			planes[RIGHT_PLANE] = Plane(corners[7], corners[5], corners[3]);
			planes[LEFT_PLANE] = Plane(corners[4], corners[6], corners[2]);

		}

		bool Frustum::Intersects(AABB aabb) {

			for (uint8_t i = 2; i < 6; i++) {

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

		bool Frustum::IsInside(AABB aabb) {

			for (uint8_t i = 2; i < 6; i++) {

				auto& normal = planes[i].normal;
				auto& distance = planes[i].distance;

				vec3 s;
				s.x = normal.x >= 0.0f ? aabb.min.x : aabb.max.x;
				s.y = normal.y >= 0.0f ? aabb.min.y : aabb.max.y;
				s.z = normal.z >= 0.0f ? aabb.min.z : aabb.max.z;

				if (distance + glm::dot(normal, s) < 0.0f)
					return false;

			}

			return true;

		}

		std::vector<vec4> Frustum::GetPlanes() {

			std::vector<vec4> planes;

			for (uint8_t i = 0; i < 6; i++) {
				planes.push_back(vec4(this->planes[i].normal,
					this->planes[i].distance));
			}

			return planes;

		}

	}

}