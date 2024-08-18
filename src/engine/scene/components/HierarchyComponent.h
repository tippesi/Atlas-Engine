#pragma once

#include "../Entity.h"
#include "../../System.h"

#include "TransformComponent.h"

namespace Atlas {

	namespace Scene {

        class Scene;

		namespace Components {

            class HierarchyComponent {

                friend Scene;

            public:
                HierarchyComponent(Scene* scene, Entity entity) : owningEntity(entity), scene(scene) {}
                HierarchyComponent(const HierarchyComponent& that) = default;

                void AddChild(Entity entity);

                void RemoveChild(Entity entity);

                std::vector<Entity>& GetChildren();

                bool root = false;

                glm::mat4 globalMatrix {1.0f};            

            protected:
                void Update(const TransformComponent& transform, bool parentChanged);

                std::vector<Entity> entities;

                bool updated = false;
                Entity owningEntity;

                Scene* scene = nullptr;

            };

		}

	}

}