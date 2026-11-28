#include "ConstraintComponent.h"
#include "../Scene.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            ConstraintComponent::ConstraintComponent(Scene* scene, Entity entity, const ConstraintComponent& that) {

                if (this != &that) {
                    *this = that;
                }

                // The constraint belongs to the copied component
                this->ref = nullptr;
                this->world = nullptr;
                this->broken = false;

                this->scene = scene;
                this->entity = entity;

            }

            ConstraintComponent::ConstraintComponent(Scene* scene, Entity entity,
                const Physics::ConstraintCreationSettings& creationSettings, Entity connectedEntity)
                : creationSettings(creationSettings), scene(scene), entity(entity), connectedEntity(connectedEntity) {

                if (!connectedEntity.IsValid())
                    this->connectedEntity = ECS::EntityConfig::InvalidEntity;

            }

            void ConstraintComponent::SetConnectedEntity(Entity connectedEntity) {

                this->connectedEntity = connectedEntity.IsValid() ? ECS::Entity(connectedEntity) :
                    ECS::EntityConfig::InvalidEntity;

                Reset();

            }

            Entity ConstraintComponent::GetConnectedEntity() const {

                AE_ASSERT(scene != nullptr && "Scene is invalid");
                return { connectedEntity, &scene->entityManager };

            }

            void ConstraintComponent::Reset() {

                RemoveFromPhysicsWorld();
                broken = false;

            }

            void ConstraintComponent::InsertIntoPhysicsWorld(Physics::BodyID bodyId, Physics::BodyID otherBodyId,
                Physics::PhysicsWorld* physicsWorld) {

                auto constraint = physicsWorld->CreateConstraint(creationSettings, bodyId, otherBodyId);

                ref = constraint.ref;
                world = physicsWorld;

            }

            void ConstraintComponent::RemoveFromPhysicsWorld() {

                if (world != nullptr)
                    world->DestroyConstraint(*this);
                ref = nullptr;
                world = nullptr;

            }

        }

    }

}
