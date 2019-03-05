#ifndef AE_DECALACTOR_H
#define AE_DECALACTOR_H

#include "Actor.h"
#include "../Decal.h"

namespace Atlas {

    namespace Actor {

        class DecalActor : public Actor {

		public:
            DecalActor(Decal* decal);

            virtual void Update(float deltaTime, mat4 parentTransform, bool parentUpdate);

            Decal* const decal;

			vec4 color = vec4(1.0f);

        };

    }

}

#endif