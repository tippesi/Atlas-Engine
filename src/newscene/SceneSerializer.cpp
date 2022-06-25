#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"

#include "../common/SerializationHelper.h"
#include "../loader/AssetLoader.h"
#include "../loader/TerrainLoader.h"

namespace Atlas {

    namespace NewScene {

        void SceneSerializer::SerializeScene(const std::string& filename) {

            YAML::Node yaml;

            yaml["version"] = "1.0";

            for (auto entity : scene->entityManager) {
                SerializeEntity(Entity { entity, scene.get() });
            }



        }

        void SceneSerializer::DeserializeScene(const std::string& filename) {

            Loader::AssetLoader::UnpackFile(filename);
            YAML::Node yaml = YAML::LoadFile(Loader::AssetLoader::GetFullPath(filename));

            if (!yaml["version"] || yaml["version"].as<std::string>() != "1.0") {
                return;
            }

            auto entityNodes = yaml["entities"];
            for (auto node : entityNodes) {
                DeserializeEntity(node);
            }

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

        void SceneSerializer::DeserializeEntity(const YAML::Node& node) {



        }

    }

}