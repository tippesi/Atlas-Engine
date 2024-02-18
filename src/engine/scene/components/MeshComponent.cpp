#include "MeshComponent.h"
#include "../Scene.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            void MeshComponent::ChangeResource(const ResourceHandle<Mesh::Mesh>& mesh) {

                AE_ASSERT(scene != nullptr && "Component needs to be added to entity before changing mesh");

                if (mesh.IsValid()) {
                    scene->UnregisterResource(scene->registeredMeshes, this->mesh);

                    this->mesh = mesh;
                }
                else {
                    this->mesh = mesh;
                }

                scene->RegisterResource(scene->registeredMeshes, mesh);

            }

        }

    }

}