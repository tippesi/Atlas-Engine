#include "Frustum.h"

namespace Atlas {

	namespace Common {

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

		bool Frustum::IsVisible(AABB aabb) {

			for (uint8_t i = 0; i < 6; i++) {

				auto normal = planes[i].normal;
				auto distance = planes[i].distance;

				vec3 s;
				s.x = normal.x >= 0.0f ? aabb.max.x : aabb.min.x;
				s.y = normal.y >= 0.0f ? aabb.max.y : aabb.min.y;
				s.z = normal.z >= 0.0f ? aabb.max.z : aabb.min.z;

				if (distance + glm::dot(normal, s) < 0.0f)
					return false;

			}

			return true;

		}

	}

}