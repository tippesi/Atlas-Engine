#pragma once

#include "../Entity.h"

#include "../components/MeshComponent.h"
#include "../components/TransformComponent.h"

namespace Atlas {

	namespace NewScene {

		namespace Prefabs {

			class MeshInstance : public Entity {

			public:
				MeshInstance(ECS::Entity entity, ECS::EntityManager* manager) : Entity(entity, manager) {

					AddComponent<Components::MeshComponent>();
					AddComponent<Components::TransformComponent>();

				}

			};

		}

	}

}