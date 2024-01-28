#pragma once

#include "../Entity.h"

#include "../components/MeshComponent.h"
#include "../components/TransformComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Prefabs {

			class MeshInstance : public Entity {

			public:
				MeshInstance(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Mesh::Mesh> mesh,
                    bool isStatic = true) : Entity(entity, manager) {

					AddComponent<MeshComponent>(mesh);
					AddComponent<TransformComponent>(mat4(1.0f), isStatic);

				}

				MeshInstance(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Mesh::Mesh> mesh,
                    mat4 transform, bool isStatic = true) : Entity(entity, manager) {

					AddComponent<MeshComponent>(mesh);
					AddComponent<TransformComponent>(transform, isStatic);

				}

			};

		}

	}

}