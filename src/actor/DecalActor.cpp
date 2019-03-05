#include "DecalActor.h"

namespace Atlas {

    namespace Actor {

        DecalActor::DecalActor(Atlas::Decal *decal) : decal(decal) {



        }

        void DecalActor::Update(float deltaTime, mat4 parentTransform, bool parentUpdate) {

            if (matrixChanged || parentUpdate) {

                matrixChanged = false;

                transformedMatrix = parentTransform * GetMatrix();

				Common::AABB base(vec3(-1.0f), vec3(1.0f));

				aabb = base.Transform(transformedMatrix);

            }

        }

    }

}