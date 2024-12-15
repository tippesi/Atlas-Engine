#pragma once

#include "../Entity.h"
#include "jobsystem/JobGroup.h"
#include "../../System.h"

#include "TransformComponent.h"
#include "CameraComponent.h"

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
                void Update(const TransformComponent& transform, bool parentChanged, ECS::Pool<TransformComponent>& transformComponentPool,
                    ECS::Pool<HierarchyComponent>& hierarchyComponentPool, ECS::Pool<CameraComponent>& cameraComponentPool);

                void Update(JobGroup& jobGroup, const TransformComponent& transform, bool parentChanged, ECS::Pool<TransformComponent>& transformComponentPool,
                    ECS::Pool<HierarchyComponent>& hierarchyComponentPool, ECS::Pool<CameraComponent>& cameraComponentPool);

                void ProcessChild(Entity entity, const TransformComponent& transform, bool parentChanged, 
                    JobGroup& jobGroup, ECS::Pool<TransformComponent>& transformComponentPool,
                    ECS::Pool<HierarchyComponent>& hierarchyComponentPool,  ECS::Pool<CameraComponent>& cameraComponentPool);

                std::vector<Entity> entities;

                bool updated = false;
                Entity owningEntity;

                Scene* scene = nullptr;

                inline static const int32_t batchSize = 16;

            };

		}

	}

}