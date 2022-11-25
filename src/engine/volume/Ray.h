#ifndef AE_RAY_H
#define AE_RAY_H

#include "../System.h"
#include "AABB.h"

namespace Atlas {

	namespace Volume {

		class Ray {

		public:
			Ray() = default;

			Ray(vec3 origin, vec3 direction);

			vec3 Get(float distance) const;

			bool Intersects(AABB aabb, float tmin, float tmax);

			bool Intersects(AABB aabb, float tmin, float tmax, float& t);

			bool Intersects(vec3 v0, vec3 v1, vec3 v2);

			bool Intersects(vec3 v0, vec3 v1, vec3 v2, vec3& intersection);

			vec3 Distance(Ray ray, float& distance);

			vec3 origin = vec3(0.0f);
			vec3 direction = vec3(0.0f, 1.0f, 0.0f);

		private:
			vec3 inverseDirection = vec3(0.0f, 1.0f, 0.0f);

		};

	}

}


#endif