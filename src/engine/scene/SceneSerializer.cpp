#include "SceneSerializer.h"
#include "Entity.h"
#include "EntitySerializer.h"

#include "../common/SerializationHelper.h"
#include "../common/SerializationHelper.h"
#include "../loader/AssetLoader.h"
#include "../loader/TerrainLoader.h"

namespace Atlas {

    namespace Scene {

        void SceneSerializer::SerializeScene(const std::string& filename) {

            auto path = Loader::AssetLoader::GetFullPath(filename);
            auto fileStream = Loader::AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

            if (!fileStream.is_open()) {
                Log::Error("Couldn't write scene file " + filename);
                return;
            }

            json j;

            std::vector<json> entities;
            for (auto entity : *scene) {
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene);
            }

            j["name"] = scene->name;
            j["aabb"] = scene->aabb;
            j["entities"] = entities;

            fileStream << to_string(j);

            fileStream.close();

        }

        void SceneSerializer::DeserializeScene(const std::string& filename) {

            Loader::AssetLoader::UnpackFile(filename);
            Loader::AssetLoader::GetFullPath(filename);

        }

        Ref<Scene> SceneSerializer::GetScene() const {

            return scene;

        }

        void SceneSerializer::SerializeEntity(Entity entity) {



        }

        void SceneSerializer::DeserializeEntity() {



        }

    }

}