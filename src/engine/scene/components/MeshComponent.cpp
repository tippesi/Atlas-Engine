#include "MeshComponent.h"
#include "../Scene.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            MeshComponent::MeshComponent(Scene* scene, Entity entity, const MeshComponent& that) {

                if (this != &that)
                    *this = that;

                this->scene = scene;
                this->entity = entity;
                // The copy is a new renderable, it is inserted with the next scene update
                inserted = false;

            }

            void MeshComponent::ChangeResource(const ResourceHandle<Mesh::Mesh>& mesh) {

                AE_ASSERT(scene != nullptr && "Component needs to be added to entity before changing mesh");

                scene->UnregisterResource(scene->registeredMeshes, this->mesh);
                this->mesh = mesh;
                scene->RegisterResource(scene->registeredMeshes, mesh);

                // Bounds and space partitioning are updated with the next scene update
                if (!resourceChanged)
                    entity.MarkDirty();
                resourceChanged = true;

            }

            void MeshComponent::SetVisible(bool visible) {

                if (this->visible == visible)
                    return;

                this->visible = visible;
                if (scene != nullptr && entity.IsValid())
                    entity.MarkDirty();

            }

            void MeshComponent::SetDontCull(bool dontCull) {

                if (this->dontCull == dontCull)
                    return;

                this->dontCull = dontCull;
                if (scene != nullptr && entity.IsValid())
                    entity.MarkDirty();

            }

        }

    }

}