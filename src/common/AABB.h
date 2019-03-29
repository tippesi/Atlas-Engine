#ifndef AE_AABB_H
#define AE_AABB_H

#include "../System.h"

namespace Atlas {

    namespace Common {

        class AABB {

        public:
            AABB() {};
            AABB(vec3 min, vec3 max);

            bool Intersects(AABB aabb);

            bool IsInside(vec3 point);

            bool IsInside(AABB aabb);

			AABB Transform(mat4 matrix);

			AABB Translate(vec3 translation);

            vec3 min = vec3(0.0f);
            vec3 max = vec3(0.0f);

        };

    }

}

#endif
