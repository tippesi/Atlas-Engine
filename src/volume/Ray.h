#ifndef AE_RAY_H
#define AE_RAY_H

#include "../System.h"
#include "AABB.h"

namespace Atlas {

	namespace Volume {

		class Ray {

		public:
			Ray() {}

			Ray(vec3 origin, vec3 direction);

			bool Intersects(AABB aabb, float tmin, float tmax);

			bool Intersects(AABB aabb, float tmin, float tmax, float& t);

			bool Intersects(vec3 v0, vec3 v1, vec3 v2);

			bool Intersects(vec3 v0, vec3 v1, vec3 v2, vec3& intersection);

			vec3 origin;
			vec3 direction = vec3(0.0f, 1.0f, 0.0f);

		private:
			vec3 inverseDirection = vec3(0.0f, 1.0f, 0.0f);

		};

	}

}


#endif