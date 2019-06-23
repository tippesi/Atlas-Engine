#include "DecalActor.h"

namespace Atlas {

    namespace Actor {

        DecalActor::DecalActor(Atlas::Decal *decal) : decal(decal) {



        }

        void DecalActor::Update(Camera, float deltaTime, 
			mat4 parentTransform, bool parentUpdate) {

            if (matrixChanged || parentUpdate) {

                matrixChanged = false;

                transformedMatrix = parentTransform * GetMatrix();

				Volume::AABB base(vec3(-1.0f), vec3(1.0f));

				aabb = base.Transform(transformedMatrix);

            }

        }

    }

}