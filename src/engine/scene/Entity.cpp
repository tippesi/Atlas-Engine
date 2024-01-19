#include "Entity.h"
#include "Scene.h"

namespace Atlas {

	namespace Scene {

		void Entity::RegisterMeshInstance(const Components::MeshComponent& meshComponent) {

            auto scene = GetScene();
            auto& mesh = meshComponent.mesh;

            if (!scene->registeredMeshes.contains(mesh.GetID()))
                scene->registeredMeshes[mesh.GetID()] = { .resource = mesh, .refCount = 1 };
            else
                scene->registeredMeshes[mesh.GetID()].refCount++;

		}

		void Entity::UnregisterMeshInstance() {

			auto scene = GetScene();

			const auto& meshComponent = GetComponent<Components::MeshComponent>();
            auto& mesh = meshComponent.mesh;

			if (mesh.IsValid() && scene->registeredMeshes.contains(mesh.GetID()))
				scene->registeredMeshes[mesh.GetID()].refCount--;

		}

		Scene* Entity::GetScene() const {

			return static_cast<Scene*>(entityManager->userData);

		}

	}

}