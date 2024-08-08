#include "Serializer.h"
#include "scene/SceneSerializer.h"

#include "lighting/LightingSerializer.h"
#include "postprocessing/PostProcessingSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "loader/AssetLoader.h"
#include "loader/TerrainLoader.h"
#include "loader/MeshLoader.h"
#include "loader/MaterialLoader.h"

#include <map>

namespace Atlas {

    void Serializer::SerializeScene(Ref<Scene::Scene> scene, const std::string& filename, bool saveDependencies, bool binaryJson,  bool formatJson) {

            auto path = Loader::AssetLoader::GetFullPath(filename);
            auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

            if (!fileStream.is_open()) {
                Log::Error("Couldn't write scene file " + filename);
                return;
            }

            json j;
            Scene::SceneToJson(j, scene.get());

            if (binaryJson) {
                auto data = json::to_bjdata(j);
                fileStream.write(reinterpret_cast<const char*>(data.data()), data.size());
            }
            else {
                fileStream << (formatJson ? j.dump(2) : j.dump());
            }

            if (saveDependencies) {
                SaveDependencies(scene);
            }

            fileStream.close();

        }

        Ref<Scene::Scene> Serializer::DeserializeScene(const std::string& filename, bool binaryJson) {

            Loader::AssetLoader::UnpackFile(filename);
            auto path = Loader::AssetLoader::GetFullPath(filename);

            auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't open scene file stream");
            }

            json j;
            if (binaryJson) {
                auto data = Loader::AssetLoader::GetFileContent(fileStream);
                j = json::from_bjdata(data);
            }
            else {
                std::string serialized((std::istreambuf_iterator<char>(fileStream)),
                    std::istreambuf_iterator<char>());
                j = json::parse(serialized);
            }
            
            fileStream.close();

            Ref<Scene::Scene> scene;
            Scene::SceneFromJson(j, scene);

            return scene;

        }

        void Serializer::SerializePrefab(Ref<Scene::Scene> scene, Scene::Entity entity, const std::string& filename, bool formatJson) {

            auto path = Loader::AssetLoader::GetFullPath(filename);
            auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

            if (!fileStream.is_open()) {
                Log::Error("Couldn't write entity file " + filename);
                return;
            }

            json j;

            std::set<ECS::Entity> insertedEntities;
            Scene::EntityToJson(j, entity, scene.get(), insertedEntities);

            auto rigidBody = entity.TryGetComponent<RigidBodyComponent>();

            if (rigidBody) {
                j["body"] = rigidBody->GetBodyCreationSettings();
            }

            fileStream << (formatJson ? j.dump(2) : j.dump());

            fileStream.close();

        }

        Scene::Entity Serializer::DeserializePrefab(Ref<Scene::Scene> scene, const std::string& filename) {

            Loader::AssetLoader::UnpackFile(filename);
            auto path = Loader::AssetLoader::GetFullPath(filename);

            auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
                throw ResourceLoadException(filename, "Couldn't open entity file stream");
            }

            std::string serialized((std::istreambuf_iterator<char>(fileStream)),
                std::istreambuf_iterator<char>());

            fileStream.close();

            json j = json::parse(serialized);

            auto entity = scene->CreateEntity();

            Scene::EntityFromJson(j, entity, scene.get());

            auto rigidBody = entity.TryGetComponent<RigidBodyComponent>();
            if (rigidBody)
                rigidBody->creationSettings = CreateRef<Physics::BodyCreationSettings>();

            if (rigidBody && j.contains("body"))
                *rigidBody->creationSettings = j["body"];

            return entity;

        }

        void Serializer::SaveDependencies(Ref<Scene::Scene> scene, bool multithreaded) {

            auto meshes = scene->GetMeshes();
            std::map<Hash, ResourceHandle<Material>> materials;

            if (multithreaded) {
                std::atomic_int32_t counter = 0;

                auto workerCount = std::thread::hardware_concurrency();
                std::vector<std::future<void>> workers;
                for (uint32_t i = 0; i < workerCount; i++) {
                    workers.emplace_back(std::async(std::launch::async, [&]() {
                        auto i = counter++;

                        while (i < int32_t(meshes.size())) {
                            auto& mesh = meshes[i];

                            if (!mesh.IsLoaded()) continue;

                            Loader::MeshLoader::SaveMesh(mesh.Get(), mesh.GetResource()->path, true);

                            i = counter++;
                        }
                        }));
                }

                for (uint32_t i = 0; i < workerCount; i++) {
                    workers[i].get();
                }
            }

            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded()) continue;

                if (!multithreaded)
                    Loader::MeshLoader::SaveMesh(mesh.Get(), mesh.GetResource()->path, true);

                for (const auto& material : mesh->data.materials)
                    materials[material.GetID()] = material;
            }

            for (const auto& [_, material] : materials) {
                if (!material.IsLoaded()) continue;

                Loader::MaterialLoader::SaveMaterial(material.Get(), material.GetResource()->path);
            }

        }

}