#ifndef AE_RAY_H
#define AE_RAY_H

#include "../System.h"
#include "AABB.h"

namespace Atlas {

	namespace Common {

		class Ray {

		public:
			Ray() {}

			Ray(vec3 origin, vec3 direction) : origin(origin), direction(direction) {}

			bool Intersects(AABB aabb);

			bool Intersects(vec3 point0, vec3 point1, vec3 point2);

			bool Intersects(vec3 point0, vec3 point1, vec3 point2, vec3& intersection);

			vec3 origin;
			vec3 direction = vec3(0.0f, 1.0f, 0.0f);

		};

	}

}


#endif