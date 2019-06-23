#include "AABB.h"

namespace Atlas {

    namespace Volume {

        AABB::AABB(vec3 min, vec3 max) : min(min), max(max) {



        }

        bool AABB::Intersects(AABB aabb) {

            return aabb.min.x <= max.x && aabb.max.x >= min.x &&
                aabb.min.y <= max.y && aabb.max.y >= min.y &&
                aabb.min.z <= max.z && aabb.max.z >= min.z;

        }

        bool AABB::IsInside(vec3 point) {

        	return point.x >= min.x && point.x <= max.x &&
        		point.y >= min.y && point.y <= max.y &&
        		point.z >= min.z && point.z <= max.z;

        }

        bool AABB::IsInside(AABB aabb) {

        	return IsInside(aabb.min) && IsInside(aabb.max);

        }

		AABB AABB::Transform(mat4 matrix) {

			vec3 cube[] = { vec3(min.x, min.y, min.z), vec3(min.x, min.y, max.z),
				vec3(max.x, min.y, min.z), vec3(max.x, min.y, max.z),
				vec3(min.x, max.y, min.z), vec3(min.x, max.y, max.z),
				vec3(max.x, max.y, min.z), vec3(max.x, max.y, max.z) };

			for (uint8_t i = 0; i < 8; i++) {
				auto homogeneous = matrix * vec4(cube[i], 1.0f);
				cube[i] = vec3(homogeneous) / homogeneous.w;
			}

			vec3 min = cube[0], max = cube[0];

			for (uint8_t i = 1; i < 8; i++) {
				min.x = glm::min(min.x, cube[i].x);
				min.y = glm::min(min.y, cube[i].y);
				min.z = glm::min(min.z, cube[i].z);

				max.x = glm::max(max.x, cube[i].x);
				max.y = glm::max(max.y, cube[i].y);
				max.z = glm::max(max.z, cube[i].z);
			}

			return AABB(min, max);

		}

		AABB AABB::Translate(vec3 translation) {

			return AABB(min + translation, max + translation);

		}

		AABB AABB::Scale(float scale) {

			auto center = 0.5f * (min + max);
			auto scaledMin = center + scale * (min - center);
			auto scaledMax = center + scale * (max - center);

			return AABB(scaledMin, scaledMax);

		}

		std::vector<vec3> AABB::GetCorners() {

			std::vector<vec3> corners;

			vec3 cube[] = { vec3(min.x, min.y, max.z), vec3(max.x, min.y, max.z),
				vec3(max.x, max.y, max.z), vec3(min.x, max.y, max.z),
				vec3(min.x, min.y, min.z), vec3(max.x, min.y, min.z),
				vec3(max.x, max.y, min.z), vec3(min.x, max.y, min.z) };

			for (uint8_t i = 0; i < 8; i++)
				corners.push_back(cube[i]);

			return corners;

		}

    }

}