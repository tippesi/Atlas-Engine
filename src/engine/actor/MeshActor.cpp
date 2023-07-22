#include "MeshActor.h"

namespace Atlas {

    namespace Actor {

        void MeshActor::Update(Camera camera, float deltaTime,
            mat4 parentTransform, bool parentUpdate) {

            lastGlobalMatrix = globalMatrix;

            if (matrixChanged || parentUpdate) {

                if (!mesh->data.IsLoaded()) {
                    // Force re-evaluation until mesh is finally loaded
                    matrixChanged = true;
                    return;
                }

                matrixChanged = false;

                globalMatrix = parentTransform * GetMatrix();

                aabb = mesh->data->aabb.Transform(globalMatrix);
            }

        }

    }

}