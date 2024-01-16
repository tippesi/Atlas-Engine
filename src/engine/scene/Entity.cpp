#include "Entity.h"
#include "Scene.h"

namespace Atlas {

	namespace Scene {

		void Entity::RegisterMeshInstance(const Components::MeshComponent& comp) {



		}

		void Entity::UnregisterMeshInstance() {

			auto scene = GetScene();

			auto& meshComponent = GetComponent<Components::MeshComponent>();

			if (meshComponent.mesh.IsValid() && scene->registeredMeshes.contains(meshComponent.mesh.GetID()))
				scene->registeredMeshes[meshComponent.mesh.GetID()].refCount--;

		}

		Scene* Entity::GetScene() const {

			return static_cast<Scene*>(entityManager->userData);

		}

	}

}