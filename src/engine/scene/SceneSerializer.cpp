#include "SceneSerializer.h"
#include "Entity.h"
#include "EntitySerializer.h"

#include "lighting/LightingSerializer.h"
#include "postprocessing/PostProcessingSerializer.h"
#include "physics/PhysicsSerializer.h"

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
            std::set<ECS::Entity> insertedEntities;

            auto hierarchySubset = scene->GetSubset<HierarchyComponent>();
            for (auto entity : hierarchySubset) {
                auto hierarchy = hierarchySubset.Get(entity);
                // Serialize from root recursively. Also means free hierarchies without a
                // root will be ignored.
                if (!hierarchy.root) continue;
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene, insertedEntities);
            }

            for (auto entity : *scene) {
                // No need here, since it was already inserted through the hierarchy
                if (insertedEntities.contains(entity))
                    continue;

                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene, insertedEntities);
            }

            // Parse all mandatory members
            j["name"] = scene->name;
            j["aabb"] = scene->aabb;
            j["depth"] = scene->depth;
            j["entities"] = entities;
            j["sky"] = scene->sky;
            j["postProcessing"] = scene->postProcessing;

            // Parse all optional members
            if (scene->fog)
                j["fog"] = *scene->fog;
            if (scene->irradianceVolume)
                j["irradianceVolume"] = *scene->irradianceVolume;
            if (scene->ao)
                j["ao"] = *scene->ao;
            if (scene->reflection)
                j["reflection"] = *scene->reflection;
            if (scene->sss)
                j["sss"] = *scene->sss;
            if (scene->ssgi)
                j["ssgi"] = *scene->ssgi;

            if (scene->physicsWorld)
                j["physicsWorld"] = *scene->physicsWorld;

            fileStream << to_string(j);

            fileStream.close();

        }

        Ref<Scene> SceneSerializer::DeserializeScene(const std::string& filename) {

            Loader::AssetLoader::UnpackFile(filename);
            auto path = Loader::AssetLoader::GetFullPath(filename);

            auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't open scene file stream");
            }

            std::string serialized((std::istreambuf_iterator<char>(fileStream)),
                std::istreambuf_iterator<char>());

            fileStream.close();

            json j = json::parse(serialized);

            Volume::AABB aabb = j["aabb"];
            auto scene = CreateRef<Scene>(j["name"], aabb.min, aabb.max, j["depth"]);

            std::vector<json> jEntities = j["entities"];
            for (auto jEntity : jEntities) {
                auto entity = scene->CreateEntity();
                EntityFromJson(jEntity, entity, scene);
            }

            scene->sky = j["sky"];
            scene->postProcessing = j["postProcessing"];

            if (j.contains("fog")) {
                scene->fog = CreateRef<Lighting::Fog>();
                *scene->fog = j["fog"];
            }
            if (j.contains("irradianceVolume")) {
                scene->irradianceVolume = CreateRef<Lighting::IrradianceVolume>();
                *scene->irradianceVolume = j["irradianceVolume"];
            }
            if (j.contains("ao")) {
                scene->ao = CreateRef<Lighting::AO>();
                *scene->ao = j["ao"];
            }
            if (j.contains("reflection")) {
                scene->reflection = CreateRef<Lighting::Reflection>();
                *scene->reflection = j["reflection"];
            }
            if (j.contains("sss")) {
                scene->sss = CreateRef<Lighting::SSS>();
                *scene->sss = j["sss"];
            }
            if (j.contains("ssgi")) {
                scene->ssgi = CreateRef<Lighting::SSGI>();
                *scene->ssgi = j["ssgi"];
            }
            if (j.contains("physicsWorld")) {
                scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
                *scene->physicsWorld = j["physicsWorld"];
            }

            scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

            return scene;

        }

        void SceneSerializer::SerializeEntity(Ref<Scene> scene, Entity entity, const std::string& filename) {



        }

        Entity SceneSerializer::DeserializeEntity(Ref<Scene> scene, const std::string& filename) {

            return Entity();

        }

    }

}