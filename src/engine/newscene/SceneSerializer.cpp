#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"

#include "../common/SerializationHelper.h"
#include "../loader/AssetLoader.h"
#include "../loader/TerrainLoader.h"

namespace Atlas {

    namespace NewScene {

        void SceneSerializer::SerializeScene(const std::string& filename) {

            for (auto entity : scene->entityManager) {
                SerializeEntity(Entity { entity, scene.get() });
            }



        }

        void SceneSerializer::DeserializeScene(const std::string& filename) {

            Loader::AssetLoader::UnpackFile(filename);
            Loader::AssetLoader::GetFullPath(filename);

        }

        Ref<Scene> SceneSerializer::GetScene() const {

            return scene;

        }

        void SceneSerializer::SerializeEntity(Entity entity) {

            if (entity.HasComponent<NameComponent>()) {

            }
            if (entity.HasComponent<TransformComponent>()) {

            }
            if (entity.HasComponent<MeshComponent>()) {

            }
            if (entity.HasComponent<LightComponent>()) {

            }
            if (entity.HasComponent<CameraComponent>()) {

            }
            if (entity.HasComponent<AudioComponent>()) {

            }
            if (entity.HasComponent<HierarchyComponent>()) {

            }

        }

        void SceneSerializer::DeserializeEntity() {



        }

    }

}