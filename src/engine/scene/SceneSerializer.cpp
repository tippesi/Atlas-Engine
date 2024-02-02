#include "SceneSerializer.h"
#include "Entity.h"
#include "EntitySerializer.h"

#include "../common/SerializationHelper.h"
#include "../common/SerializationHelper.h"
#include "../loader/AssetLoader.h"
#include "../loader/TerrainLoader.h"

namespace Atlas {

    namespace Scene {

        void SceneSerializer::SerializeScene(Ref<Scene> scene, const std::string& filename) {

            auto path = Loader::AssetLoader::GetFullPath(filename);
            auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

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

        Ref<Scene> SceneSerializer::DeserializeScene(const std::string& filename) {

            Loader::AssetLoader::UnpackFile(filename);
            auto path = Loader::AssetLoader::GetFullPath(filename);

            auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

            if (!fileStream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't open scene file stream");
            }

            json j;

            /*
            j << fileStream;

            std::vector<json> entities;
            for (auto entity : *scene) {
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene);
            }

            j["name"] = scene->name;
            j["aabb"] = scene->aabb;
            j["depth"] = scene->d;
            j["entities"] = entities;

            auto scene = CreateRef<Scene>();

            fileStream << to_string(j);

            fileStream.close();
            */

            return nullptr;

        }

        void SceneSerializer::SerializeEntity(Ref<Scene> scene, Entity entity, const std::string& filename) {



        }

        Entity SceneSerializer::DeserializeEntity(Ref<Scene> scene, const std::string& filename) {

            return Entity();

        }

    }

}