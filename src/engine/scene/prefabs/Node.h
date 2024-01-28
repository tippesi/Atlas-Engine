#pragma once

#include "../Entity.h"

#include "../components/HierarchyComponent.h"
#include "../components/TransformComponent.h"
#include "../components/NameComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Prefabs {

			class Node : public Entity {

			public:
				Node(ECS::Entity entity, ECS::EntityManager* manager, mat4 transform) : Entity(entity, manager) {

					AddComponent<HierarchyComponent>();
					AddComponent<TransformComponent>();
					AddComponent<NameComponent>();

				}

			};

		}

	}

}