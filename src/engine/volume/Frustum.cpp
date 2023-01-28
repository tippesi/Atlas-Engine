#include "Frustum.h"

namespace Atlas {

	namespace Volume {

		Frustum::Frustum(const std::vector<vec3>& corners) {

			Resize(corners);

		}

		Frustum::Frustum(mat4 matrix) {

			Resize(matrix);

		}

		void Frustum::Resize(std::vector<vec3> corners) {

			this->corners = corners;
			planes[NEAR_PLANE] = Plane(corners[4], corners[5], corners[7]);
			planes[FAR_PLANE] = Plane(corners[1], corners[0], corners[2]);
			planes[TOP_PLANE] = Plane(corners[5], corners[4], corners[0]);
			planes[BOTTOM_PLANE] = Plane(corners[6], corners[7], corners[3]);
			planes[RIGHT_PLANE] = Plane(corners[7], corners[5], corners[3]);
			planes[LEFT_PLANE] = Plane(corners[4], corners[6], corners[2]);

		}

		void Frustum::Resize(mat4 matrix) {

			CalculateCorners(matrix);
			Resize(corners);

		}

		bool Frustum::Intersects(AABB aabb) {

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

		bool Frustum::IsInside(AABB aabb) {

			for (uint8_t i = 0; i < 6; i++) {

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

		std::vector<vec3> Frustum::GetCorners() {

			return corners;

		}

		void Frustum::CalculateCorners(mat4 matrix) {

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

			corners.clear();
			auto inverseMatrix = glm::inverse(matrix);

			for (uint8_t i = 0; i < 8; i++) {
				auto homogenous = inverseMatrix * vec4(vectors[i], 1.0f);
				corners.push_back(vec3(homogenous) / homogenous.w);
			}

		}

	}

}