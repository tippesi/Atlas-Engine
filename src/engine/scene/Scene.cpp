#include "Scene.h"
#include "Entity.h"
#include "components/Components.h"

namespace Atlas {

    namespace Scene {

        using namespace Components;

        Scene::~Scene() {

            Clear();

        }

        Entity Scene::CreateEntity() {

            return ToSceneEntity(entityManager.Create());

        }

        void Scene::DestroyEntity(Entity entity) {

            entityManager.Destroy(entity);

        }

        void Scene::Merge(const Ref<Scene> &other) {

            std::unordered_map<ECS::Entity, ECS::Entity> entityToEntityMap;

            // We need all entities before we can start to attach components
            for(auto entity : *other) {
                auto newEntity = CreateEntity();

                entityToEntityMap[entity] = newEntity;
            }

            for (auto entity : *other) {
                auto newEntity = entityToEntityMap[entity];

                if (entity.HasComponent<HierarchyComponent>()) {

                }
                if (entity.HasComponent<TransformComponent>()) {

                }
            }



        }

        void Scene::Update(Ref<Camera> camera, float deltaTime) {

            Update(deltaTime);
            UpdateCameraDependent(camera, deltaTime);

        }

        void Scene::Update(float deltaTime) {

            // Do cleanup first such that we work with valid data
            CleanupUnusedResources();

            auto hierarchySubset = entityManager.GetSubset<HierarchyComponent, TransformComponent>();
            // Update hierarchy and their entities
            for (auto entity : hierarchySubset) {
                const auto& [hierarchyComponent, transformComponent] = hierarchySubset.Get(entity);

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

            // Wait for transform updates to finish
            if (physicsWorld != nullptr) {
                auto rigidBodySubset = entityManager.GetSubset<RigidBodyComponent, TransformComponent>();

                for (auto entity : rigidBodySubset) {
                    const auto& [rigidBodyComponent, transformComponent] = rigidBodySubset.Get(entity);

                    // Might happen if there was no transform at the creation of rigid body component
                    if (!rigidBodyComponent.Valid()) {
                        rigidBodyComponent.InsertIntoPhysicsWorld(transformComponent, physicsWorld.get());
                    }

                    // Apply update here (transform overwrite everything else in physics simulation for now)
                    if (transformComponent.changed && rigidBodyComponent.Valid()) {
                        rigidBodyComponent.SetMatrix(transformComponent.globalMatrix);
                    }
                }

                physicsWorld->Update(deltaTime);

                for (auto entity : rigidBodySubset) {
                    const auto& [rigidBodyComponent, transformComponent] = rigidBodySubset.Get(entity);

                    if (!rigidBodyComponent.Valid() || transformComponent.isStatic ||
                        rigidBodyComponent.layer == Physics::Layers::STATIC)
                        continue;

                    // Need to set changed to true such that the space partitioning is updated
                    transformComponent.changed = true;
                    transformComponent.updated = true;

                    // Physics are updated in global space, so we don't need the parent transform
                    transformComponent.globalMatrix = rigidBodyComponent.GetMatrix();
                    transformComponent.inverseGlobalMatrix = glm::inverse(transformComponent.globalMatrix);
                }
            }

            // Do the space partitioning update here (ofc also update AABBs)
            auto meshSubset = entityManager.GetSubset<MeshComponent, TransformComponent>();
            for (auto entity : meshSubset) {
                auto& meshComponent = entityManager.Get<MeshComponent>(entity);

                if (!meshComponent.mesh.IsLoaded())
                    continue;

                auto& transformComponent = entityManager.Get<TransformComponent>(entity);
                if (!transformComponent.changed && meshComponent.inserted)
                    continue;

                if (meshComponent.inserted)
                    SpacePartitioning::RemoveRenderableEntity(ToSceneEntity(entity), meshComponent);

                meshComponent.aabb = meshComponent.mesh->data.aabb.Transform(transformComponent.globalMatrix);

                SpacePartitioning::InsertRenderableEntity(ToSceneEntity(entity), meshComponent);
                meshComponent.inserted = true;
            }

            // After everything we need to reset transform component changed and prepare the updated for next frame
            for (auto entity : transformSubset) {
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);

                transformComponent.changed = false;
                transformComponent.updated = false;
            }

#ifdef AE_BINDLESS
            UpdateBindlessIndexMaps();

            // Make sure this is changed just once at the start of a frame
            if (rayTracingWorld) {
                rayTracingWorld->scene = this;
                rayTracingWorld->Update(true);
            }
            rtDataValid = rayTracingWorld != nullptr && rayTracingWorld->IsValid();
#endif

        }

        void Scene::UpdateCameraDependent(Ref<Camera> camera, float deltaTime) {

            if (terrain) {
                terrain->Update(camera.get());
            }

            if (ocean) {
                ocean->Update(camera.get(), deltaTime);
            }

            if (sky.sun) {
                sky.sun->Update(camera.get());
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

                for (const auto& material : terrainMaterials) {
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

            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;
                for (auto& material : mesh->data.materials) {
                    materials.push_back(material.get());
                }
            }

            return materials;

        }

        void Scene::GetRenderList(Volume::Frustum frustum, Atlas::RenderList &renderList) {

            // This is much quicker presumably due to cache coherency (need better hierarchical data structure)
            auto subset = entityManager.GetSubset<Components::MeshComponent>();
            for (auto& entity : subset) {
                auto& comp = subset.Get(entity);

                if (comp.dontCull || comp.visible && frustum.Intersects(comp.aabb))
                    renderList.Add(entity, comp);
            }

        }

        void Scene::ClearRTStructures() {

            rtDataValid = false;
            if (rayTracingWorld != nullptr)
                rayTracingWorld->Clear();

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

            for (const auto& mesh : meshes) {
                loaded &= mesh.IsLoaded();
            }

            return loaded;

        }

        bool Scene::IsRtDataValid() const {

            return rtDataValid;

        }

        void Scene::Clear() {

            ClearRTStructures();
            entityManager.Clear();

            CleanupUnusedResources();

            // Clean up stuff that components haven't done by themselves (should not happen, inform in debug mode)
                AE_ASSERT(registeredMeshes.empty() && "Registered meshes should be emtpy after cleanup");
            registeredMeshes.clear();

        }

        SceneIterator Scene::begin() {

            return { &entityManager, 0 };

        }

        SceneIterator Scene::end() {

            return { &entityManager, size_t(entityManager.end() - entityManager.begin()) };

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

        void Scene::CleanupUnusedResources() {

            CleanupUnusedResources(registeredMeshes);

        }

        Entity Scene::ToSceneEntity(ECS::Entity entity) {

            return { entity, &entityManager };

        }

        void Scene::RegisterSubscribers() {

            // Each resource type needs to count references
            entityManager.SubscribeToTopic<MeshComponent>(ECS::Topic::ComponentEmplace,
                [this](const ECS::Entity entity, MeshComponent& meshComponent)  {
                    RegisterResource(registeredMeshes, meshComponent.mesh);
                });

            entityManager.SubscribeToTopic<MeshComponent>(ECS::Topic::ComponentErase,
                [this](const ECS::Entity entity, MeshComponent& meshComponent)  {
                    UnregisterResource(registeredMeshes, meshComponent.mesh);

                    if (meshComponent.inserted) {
                        SpacePartitioning::RemoveRenderableEntity(ToSceneEntity(entity), meshComponent);
                    }
                });

            // Need insert/remove physics components into physics world
            entityManager.SubscribeToTopic<RigidBodyComponent>(ECS::Topic::ComponentEmplace,
                [this](const ECS::Entity entity, RigidBodyComponent& rigidBodyComponent)  {
                    auto transformComp = entityManager.GetIfContains<TransformComponent>(entity);
                    if (!transformComp) return;

                    if (physicsWorld != nullptr)
                        rigidBodyComponent.InsertIntoPhysicsWorld(*transformComp, physicsWorld.get());
                });

            entityManager.SubscribeToTopic<RigidBodyComponent>(ECS::Topic::ComponentErase,
                [this](const ECS::Entity entity, RigidBodyComponent& rigidBodyComponent)  {
                    if (physicsWorld != nullptr)
                        rigidBodyComponent.RemoveFromPhysicsWorld();
                });

        }

    }

}