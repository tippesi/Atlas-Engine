#pragma once

#include "../Entity.h"

#include "../components/MeshComponent.h"
#include "../components/TransformComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Prefabs {

			class MeshInstance : public Entity {

			public:
				MeshInstance(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Mesh::Mesh> mesh) : Entity(entity, manager) {

					AddComponent<Components::MeshComponent>(mesh);
					AddComponent<Components::TransformComponent>();

				}

				MeshInstance(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Mesh::Mesh> mesh, mat4 transform) : Entity(entity, manager) {

					AddComponent<Components::MeshComponent>(mesh);
					AddComponent<Components::TransformComponent>(transform);

				}

			};

		}

	}

}