#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"

#include "../Entity.h"

#include <nlohmann/json_fwd.hpp>

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            // Connects the rigid body of the entity to the rigid body of the connected entity or to the world
            class ConstraintComponent : public Physics::Constraint {

            public:
                ConstraintComponent() = default;
                ConstraintComponent(Scene* scene, Entity entity) : scene(scene), entity(entity) {}
                ConstraintComponent(Scene* scene, Entity entity, const ConstraintComponent& that);
                explicit ConstraintComponent(Scene* scene, Entity entity, const Physics::ConstraintCreationSettings& creationSettings,
                    Entity connectedEntity = Entity());

                void SetConnectedEntity(Entity connectedEntity);

                Entity GetConnectedEntity() const;

                // Creates the constraint again with the current settings, also after it broke
                void Reset();

                Physics::ConstraintCreationSettings creationSettings;

                bool broken = false;

            private:
                void InsertIntoPhysicsWorld(Physics::BodyID bodyId, Physics::BodyID otherBodyId,
                    Physics::PhysicsWorld* physicsWorld);

                void RemoveFromPhysicsWorld();

                Scene* scene = nullptr;
                ECS::Entity entity = ECS::EntityConfig::InvalidEntity;
                ECS::Entity connectedEntity = ECS::EntityConfig::InvalidEntity;

                friend Scene;
                friend void to_json(nlohmann::json& j, const ConstraintComponent& p);
                friend void from_json(const nlohmann::json& j, ConstraintComponent& p);

            };

        }

    }

}
