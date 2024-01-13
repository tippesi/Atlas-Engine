#pragma once

#include "../Entity.h"

#include "../components/HierarchyComponent.h"
#include "../components/TransformComponent.h"
#include "../components/NameComponent.h"

namespace Atlas {

	namespace NewScene {

		namespace Prefabs {

			class Node : public Entity {

			public:
				Node(ECS::Entity entity, Scene* scene, mat4 transform) : Entity(entity, scene) {

					AddComponent<Components::HierarchyComponent>();
					AddComponent<Components::TransformComponent>();
					AddComponent<Components::NameComponent>();

				}

			};

		}

	}

}