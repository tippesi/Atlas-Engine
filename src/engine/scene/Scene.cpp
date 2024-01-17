#include "Scene.h"
#include "Entity.h"
#include "components/Components.h"

namespace Atlas {

    namespace Scene {

        using namespace Components;

        Entity Scene::CreateEntity() {

            return Entity(entityManager.Create(), &entityManager);

        }

        void Scene::DestroyEntity(Entity entity) {

            entityManager.Destroy(entity);

        }

        void Scene::Update(Ref<Camera> camera, float deltaTime) {

            Update(deltaTime);
            UpdateCameraDependent(camera, deltaTime);

        }

        void Scene::Update(float deltaTime) {

            auto hierarchySubset = entityManager.GetSubset<HierarchyComponent, TransformComponent>();
            // Update hierarchy and their entities
            for (auto entity : hierarchySubset) {
                auto& hierarchyComponent = entityManager.Get<HierarchyComponent>(entity);
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);

                if (hierarchyComponent.root) {
                    hierarchyComponent.Update(transformComponent, false);
                }
            }

            auto transformSubset = entityManager.GetSubset<TransformComponent>();

            TransformComponent rootTransform = {};
            // Update all other transforms not affected by the hierarchy (entities don't need to be in hierarchy)
            for (auto entity : transformSubset) {
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);

                if (!transformComponent.updated) {
                    transformComponent.Update(rootTransform, false);
                }
            }

            auto meshSubset = entityManager.GetSubset<MeshComponent, TransformComponent>();
            for (auto entity : meshSubset) {
                auto& meshComponent = entityManager.Get<MeshComponent>(entity);

                if (!meshComponent.mesh.IsLoaded())
                    continue;

                auto& transformComponent = entityManager.Get<TransformComponent>(entity);
                if (!transformComponent.changed && meshComponent.inserted)
                    continue;

                if (meshComponent.inserted)
                    SpacePartitioning::RemoveRenderableEntity(Entity(entity, &entityManager), meshComponent);

                meshComponent.aabb = meshComponent.mesh->data.aabb.Transform(transformComponent.globalMatrix);

                SpacePartitioning::InsertRenderableEntity(Entity(entity, &entityManager), meshComponent);
                meshComponent.inserted = true;
            }            

            for (auto entity : transformSubset) {
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);

                transformComponent.changed = false;
            }

        }

        void Scene::UpdateCameraDependent(Ref<Camera> camera, float deltaTime) {

            if (terrain) {
                terrain->Update(camera.get());
            }

            if (ocean) {
                ocean->Update(camera.get(), deltaTime);
            }

        }

        std::vector<ResourceHandle<Mesh::Mesh>> Scene::GetMeshes() {

            std::vector<ResourceHandle<Mesh::Mesh>> meshes;

            // Not really efficient, but does the job
            for (auto& [meshId, registeredMesh] : registeredMeshes) {
                meshes.push_back(registeredMesh.resource);
            }

            return meshes;

        }

        std::vector<Material*> Scene::GetMaterials() {

            std::vector<Material*> materials;

            if (terrain) {
                auto terrainMaterials = terrain->storage.GetMaterials();

                for (auto material : terrainMaterials) {
                    if (!material)
                        continue;

                    materials.push_back(material.get());
                }

            }

            auto meshes = GetMeshes();
            if (vegetation) {
                auto vegMeshes = vegetation->GetMeshes();
                meshes.insert(meshes.end(), vegMeshes.begin(), vegMeshes.end());
            }

            for (auto mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;
                for (auto& material : mesh->data.materials) {
                    materials.push_back(material.get());
                }
            }

            return materials;

        }

        void Scene::ClearRTStructures() {

            rtDataValid = false;
            rtData.Clear();

        }

        void Scene::WaitForResourceLoad() {

            auto meshes = GetMeshes();

            for (auto mesh : meshes) {
                mesh.WaitForLoad();
            }

        }

        bool Scene::IsFullyLoaded() {

            bool loaded = true;

            auto meshes = GetMeshes();

            for (auto mesh : meshes) {
                loaded &= mesh.IsLoaded();
            }

            return loaded;

        }

        bool Scene::IsRtDataValid() {

            return rtDataValid;

        }

        void Scene::UpdateBindlessIndexMaps() {

            std::set<Ref<Texture::Texture2D>> textures;

            uint32_t textureIdx = 0;
            uint32_t bufferIdx = 0;

            textureToBindlessIdx.clear();
            meshIdToBindlessIdx.clear();

            auto meshes = GetMeshes();
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded()) continue;

                for (auto& material : mesh->data.materials) {
                    if (material->HasBaseColorMap())
                        textures.insert(material->baseColorMap);
                    if (material->HasOpacityMap())
                        textures.insert(material->opacityMap);
                    if (material->HasNormalMap())
                        textures.insert(material->normalMap);
                    if (material->HasRoughnessMap())
                        textures.insert(material->roughnessMap);
                    if (material->HasMetalnessMap())
                        textures.insert(material->metalnessMap);
                    if (material->HasAoMap())
                        textures.insert(material->aoMap);
                }

                // Not all meshes might have a bvh
                if (!mesh->IsBVHBuilt())
                    continue;

                meshIdToBindlessIdx[mesh.GetID()] = bufferIdx++;
            }

            for (const auto& texture : textures) {

                textureToBindlessIdx[texture] = textureIdx++;

            }

        }

    }

}