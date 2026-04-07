#include "Scene.h"
#include "Entity.h"
#include "SceneSerializer.h"
#include "components/Components.h"
#include "components/LuaScriptComponent.h"

namespace Atlas {

    namespace Scene {

        using namespace Components;

        Scene::~Scene() {

            Clear();
            WaitForAsyncWorkCompletion();

        }

        Entity Scene::CreateEntity() {

            return ToSceneEntity(entityManager.Create());

        }

        void Scene::DestroyEntity(Entity entity, bool removeRecursively, bool parentDeleted) {

            auto parentEntity = GetParentEntity(entity);

            if (!parentDeleted && parentEntity.IsValid()) {
                auto& hierarchyComponent = parentEntity.GetComponent<HierarchyComponent>();
                hierarchyComponent.RemoveChild(entity);
            }

            if (removeRecursively) {
                auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
                if (hierarchyComponent) {
                    auto children = hierarchyComponent->GetChildren();
                    for (auto childEntity : children) {
                        DestroyEntity(childEntity, removeRecursively, true);
                    }
                }
            }

            entityManager.Destroy(entity);

        }

        Entity Scene::DuplicateEntity(Entity entity) {

            auto newEntity = CreateEntity();

            DuplicateEntityComponents(entity, newEntity, nullptr);

            return newEntity;

        }

        size_t Scene::GetEntityCount() const {

            return entityManager.Alive();

        }

        Entity Scene::GetEntityByName(const std::string& name) {

            auto nameSubset = entityManager.GetSubset<NameComponent>();
            for (auto entity : nameSubset) {
                const auto& nameComponent = nameSubset.Get(entity);

                if (nameComponent.name == name)
                    return { entity, &entityManager };
            }

            return { ECS::EntityConfig::InvalidEntity, &entityManager };

        }

        Entity Scene::GetParentEntity(Entity entity) {

            auto iter = childToParentMap.find(entity);

            if (iter != childToParentMap.end()) {
                return { iter->second, &entityManager };
            }

            return { ECS::EntityConfig::InvalidEntity, &entityManager };

        }

        void Scene::Update(float deltaTime) {

            this->deltaTime = deltaTime;

            WaitForAsyncWorkCompletion();

            // Do cleanup first such that we work with valid data
            CleanupUnusedResources();

            // Update scripting components (but only after the first timestep when everything else is settled)
            if (!firstTimestep) {
                // Work with a copy here
                auto luaScriptComponents = entityManager.GetAll<LuaScriptComponent>();
                for (auto& luaScriptComponent : luaScriptComponents) {
                    luaScriptComponent.Update(luaScriptManager, deltaTime);

                    // Replace original one with the updated component
                    auto& originalComponent = entityManager.Get<LuaScriptComponent>(luaScriptComponent.entity);
                    originalComponent = luaScriptComponent;
                }
            }

            TransformComponent rootTransform = {};

            auto& transformComponentPool = entityManager.GetPool<TransformComponent>();
            auto& hierarchyComponentPool = entityManager.GetPool<HierarchyComponent>();
            auto& cameraComponentPool = entityManager.GetPool<CameraComponent>();
            auto& meshComponentPool = entityManager.GetPool<MeshComponent>();

            auto hierarchySubset = entityManager.GetSubset<HierarchyComponent>();

            JobGroup terrainOceanJobGroup{ "Terrain and ocean update", JobPriority::High};

            // Do these updates before anything else (doesn't have newest camera position, but that doesn't matter too much
            if (HasMainCamera()) {
                auto& mainCamera = mainCameraEntity.GetComponent<CameraComponent>();

                if (terrain.IsLoaded()) {
                    JobSystem::Execute(terrainOceanJobGroup, [&](JobData&) {
                        terrain->Update(mainCamera);
                        });
                }

                if (ocean) {
                    JobSystem::Execute(terrainOceanJobGroup, [&](JobData&) {
                        ocean->Update(mainCamera, deltaTime);
                        });
                }
            }

            // Kick of all the jobs while we find the changed transforms
            FindChangedTransforms([&]() {
                JobSystem::WaitSpin(terrainOceanJobGroup);

                // Can only update after scripts were runxx
                renderState.NewFrame();
                if (Graphics::GraphicsDevice::DefaultDevice->support.bindless) {
                    renderState.UpdateBlasBindlessData();
                    renderState.UpdateTextureBindlessData();
                    renderState.UpdateOtherTextureBindlessData();
                }
                renderState.PrepareMaterials();

                });

            JobGroup jobGroup{ "Scene update", JobPriority::High};

            // Start the hierarchy update as early as possible in the frame (we need to move this further down if we need terrain info in the future)
            // Update hierarchy and their entities
            for (const auto& [entity, level] : changedTransformSet) {
                if (level < 0)
                    break;

                auto& hierarchyComponent = hierarchyComponentPool.Get(entity);
                if (hierarchyComponent.updated)
                    continue;

                glm::mat4 parentMatrix(1.0f);
                auto parentEntity = GetParentEntity(Entity(entity, &entityManager));
                if (parentEntity.IsValid()) {
                    auto& hierarchyComponent = hierarchyComponentPool.Get(parentEntity);
                    parentMatrix = hierarchyComponent.globalMatrix;
                }
                
                auto& transformComponent = transformComponentPool.Get(entity);
                transformComponent.Update(parentMatrix, true);
                hierarchyComponent.Update(jobGroup, transformComponent, true,
                    transformComponentPool, hierarchyComponentPool, cameraComponentPool);

                JobSystem::Wait(jobGroup);
            }

            JobSystem::ParallelFor(jobGroup, int32_t(changedTransformSet.size()), 8, [&](JobData&, int32_t idx) {
                const auto& [entity, level] = changedTransformSet[idx];
                if (level >= 0)
                    return;

                auto& transformComponent = transformComponentPool.Get(entity);
                if (transformComponent.updated)
                    return;

                glm::mat4 parentMatrix(1.0f);
                auto parentEntity = GetParentEntity(Entity(entity, &entityManager));
                if (parentEntity.IsValid()) {
                    auto& hierarchyComponent = hierarchyComponentPool.Get(parentEntity);
                    parentMatrix = hierarchyComponent.globalMatrix;
                }

                transformComponent.Update(parentMatrix, true);
                });

            JobSystem::Wait(jobGroup);

            // Wait for transform updates to finish
            if (physicsWorld != nullptr) {

                // This part (updating physics transforms) is required regardless of the simulation running
                auto playerSubset = entityManager.GetSubset<PlayerComponent, TransformComponent>();
                for (auto entity : playerSubset) {
                    const auto& [playerComponent, transformComponent] = playerSubset.Get(entity);

                    // Might happen if there was no transform at the creation of rigid body component
                    if (!playerComponent.IsValid()) {
                        playerComponent.InsertIntoPhysicsWorld(transformComponent, physicsWorld.get());
                    }

                    // Apply update here (transform overwrite everything else in physics simulation for now)
                    if (transformComponent.changed && playerComponent.IsValid()) {
                        auto& globalPlayerMatrix = transformComponent.globalMatrix;
                        auto decomposition = Common::MatrixDecomposition(globalPlayerMatrix);
                        // In theory we have to apply the rotation here as well
                        playerComponent.SetPosition(decomposition.translation);
                    }

                    playerComponent.Update(deltaTime);

                    auto hierarchyComponent = hierarchyComponentPool.TryGet(entity);
                    if (hierarchyComponent) {
                        hierarchyComponent->Update(transformComponent, true,
                            transformComponentPool, hierarchyComponentPool, cameraComponentPool);
                    }
                }

                auto rigidBodySubset = entityManager.GetSubset<RigidBodyComponent, TransformComponent>();

                for (auto entity : rigidBodySubset) {
                    const auto& [rigidBodyComponent, transformComponent] = rigidBodySubset.Get(entity);

                    // Might happen if there was no transform at the creation of rigid body component
                    if (!rigidBodyComponent.IsValid()) {
                        rigidBodyComponent.InsertIntoPhysicsWorld(transformComponent, physicsWorld.get());
                    }

                    // Apply update here (transform overwrite everything else in physics simulation for now)
                    if (transformComponent.changed && rigidBodyComponent.IsValid()) {
                        rigidBodyComponent.SetMatrix(transformComponent.globalMatrix);
                    }
                }

                // This part only needs to be executed if the simulation is running
                if (!physicsWorld->pauseSimulation) {

                    physicsWorld->Update(deltaTime);

                    for (auto entity : rigidBodySubset) {
                        const auto& [rigidBodyComponent, transformComponent] = rigidBodySubset.Get(entity);

                        if (!rigidBodyComponent.IsValid() || transformComponent.isStatic ||
                            rigidBodyComponent.layer == Physics::Layers::Static)
                            continue;

                        // This happens if no change was triggered by the user, then we still need
                        // to update the last global matrix, since it might have changed due to physics simulation
                        if (!transformComponent.changed)
                            transformComponent.lastGlobalMatrix = transformComponent.globalMatrix;

                        // Need to set changed to true such that the space partitioning is updated
                        transformComponent.changed = true;
                        transformComponent.updated = true;

                        // Physics are updated in global space, so we don't need the parent transform
                        transformComponent.globalMatrix = rigidBodyComponent.GetMatrix();
                        transformComponent.inverseGlobalMatrix = glm::inverse(transformComponent.globalMatrix);
                    }

                    // Player update needs to be performed after normal rigid bodies, such that
                    // player can override rigid body behaviour
                    for (auto entity : playerSubset) {
                        const auto& [playerComponent, transformComponent] = playerSubset.Get(entity);

                        if (!playerComponent.IsValid())
                            continue;

                        // This happens if no change was triggered by the user, then we still need
                        // to update the last global matrix, since it might have changed due to physics simulation
                        if (!transformComponent.changed)
                            transformComponent.lastGlobalMatrix = transformComponent.globalMatrix;

                        // Need to set changed to true such that the space partitioning is updated
                        transformComponent.changed = true;
                        transformComponent.updated = true;

                        // Physics are updated in global space, so we don't need the parent transform
                        transformComponent.globalMatrix = playerComponent.GetMatrix();
                        transformComponent.inverseGlobalMatrix = glm::inverse(transformComponent.globalMatrix);
                    }

                    // Now we need to update all the hiearchies
                    auto rigidBodyHierarchySubset = entityManager.GetSubset<RigidBodyComponent, HierarchyComponent, TransformComponent>();
                    for (auto entity : rigidBodyHierarchySubset) {
                        auto& hierarchyComponent = hierarchyComponentPool.Get(entity);

                        hierarchyComponent.updated = false;
                    }

                    for (auto entity : rigidBodyHierarchySubset) {
                        const auto& [rigidBodyComponent, hierarchyComponent, transformComponent] = rigidBodyHierarchySubset.Get(entity);

                        if (!hierarchyComponent.updated) {
                            auto parentChanged = transformComponent.changed;
                            hierarchyComponent.Update(transformComponent, parentChanged,
                                transformComponentPool, hierarchyComponentPool, cameraComponentPool);
                        }
                    }

                    // Now we need to update all the hiearchies
                    auto playerHierarchySubset = entityManager.GetSubset<PlayerComponent, HierarchyComponent, TransformComponent>();
                    for (auto entity : playerHierarchySubset) {
                        auto& hierarchyComponent = hierarchyComponentPool.Get(entity);

                        hierarchyComponent.updated = false;
                    }

                    for (auto entity : playerHierarchySubset) {
                        const auto& [playerComponent, hierarchyComponent, transformComponent] = playerHierarchySubset.Get(entity);

                        if (!hierarchyComponent.updated) {
                            auto parentChanged = transformComponent.changed;
                            hierarchyComponent.Update(transformComponent, parentChanged,
                                transformComponentPool, hierarchyComponentPool, cameraComponentPool);
                        }
                    }
                }
            }

            // Do camera update while we find changed meshes
            FindChangedMeshes([&]() {
                // Don't do anything in parallel here, we don't expect to have many cameras
                mainCameraEntity = Entity();
                auto cameraSubset = entityManager.GetSubset<CameraComponent>();
                // Attempt to find a main camera
                for (auto entity : cameraSubset) {
                    auto& camera = cameraSubset.Get(entity);

                    mat4 transformMatrix = mat4(1.0f);
                    auto transform = entityManager.TryGet<TransformComponent>(entity);
                    if (transform) {
                        camera.parentTransform = transform->globalMatrix;
                        transformMatrix = transform->globalMatrix;
                    }

                    camera.Update(transformMatrix);

                    if (camera.isMain && !mainCameraEntity.IsValid()) {
                        mainCameraEntity = { entity, &entityManager };
                    }
                }
                });

            // Do the space partitioning update here (ofc also update AABBs)
            for (auto entity : changedMeshSet) {
                auto& meshComponent = meshComponentPool.Get(entity);
                auto& transformComponent = transformComponentPool.Get(entity);

                if (meshComponent.inserted)
                    SpacePartitioning::RemoveRenderableEntity(ToSceneEntity(entity), meshComponent);

                meshComponent.aabb = meshComponent.mesh->data.aabb.Transform(transformComponent.globalMatrix);

                SpacePartitioning::InsertRenderableEntity(ToSceneEntity(entity), meshComponent);
                meshComponent.inserted = true;
            }

            renderState.FillMainRenderPass();

            JobGroup lightJobGroup{ "Lights update", JobPriority::High };
            auto& lightComponentPool = entityManager.GetPool<LightComponent>();
            JobSystem::ParallelFor(lightJobGroup, int32_t(lightComponentPool.GetCount()), 4, [&](JobData&, int32_t idx) {
                auto& lightComponent = lightComponentPool.GetByIndex(idx);
                auto entity = lightComponentPool[idx];

                // This will lead to threading issues if there are several main lights (which there shouldn't be!)
                if (lightComponent.isMain && lightComponent.type == LightType::DirectionalLight)
                    mainLightEntity = Entity(entity, &entityManager);

                auto transformComponent = transformComponentPool.TryGet(entity);

                lightComponent.Update(transformComponent);

                if (mainCameraEntity.IsValid()) {
                    auto& mainCamera = GetMainCamera();
                    lightComponent.Update(mainCamera);

                    renderState.FillShadowRenderPass(Entity(entity, &entityManager));
                }
                });

            JobSystem::Wait(lightJobGroup);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            if (device->support.bindless) {
                auto rayTracingSubset = GetSubset<MeshComponent, TransformComponent>();
                JobSystem::Execute(renderState.rayTracingWorldUpdateJob, [this, rayTracingSubset](JobData&) {
                    // Check if any rt effects are running (pathtracing checked by rayTracingWorld->includeObjectHistory)
                    auto rtUpdateNeeded = (irradianceVolume && irradianceVolume->enable) ||
                        (reflection && reflection->enable && reflection->rt) ||
                        (rayTracingWorld && rayTracingWorld->includeObjectHistory) ||
                        (rtgi && rtgi->enable);
                    // Need to wait before updating graphic resources
                    Graphics::GraphicsDevice::DefaultDevice->WaitForPreviousFrameSubmission();
                    if (rayTracingWorld && rtUpdateNeeded) {
                        rayTracingWorld->scene = this;
                        // Don't update triangle lights for now (second argument)
                        rayTracingWorld->Update(rayTracingSubset, false);
                    }
                    rtDataValid = rayTracingWorld != nullptr && rayTracingWorld->IsValid();
                });
            }

            if (HasMainCamera()) {
                auto& mainCamera = GetMainCamera();

                auto& audioComponentPool = entityManager.GetPool<AudioComponent>();
                JobSystem::ParallelFor(jobGroup, int32_t(audioComponentPool.GetCount()), 4, [&](JobData&, int32_t idx) {
                    auto entity = audioComponentPool[idx];
                    auto& audioComponent = audioComponentPool.GetByIndex(idx);
                    auto transformComponent = transformComponentPool.TryGet(entity);

                    if (!transformComponent)
                        return;

                    audioComponent.Update(deltaTime, *transformComponent, mainCamera.GetLocation(),
                        mainCamera.GetLastLocation(), mainCamera.right);
                    });

                auto& audioVolumeComponentPool = entityManager.GetPool<AudioVolumeComponent>();
                JobSystem::ParallelFor(jobGroup, int32_t(audioVolumeComponentPool.GetCount()), 4, [&](JobData&, int32_t idx) {
                    auto entity = audioVolumeComponentPool[idx];
                    auto& audioComponent = audioVolumeComponentPool.GetByIndex(idx);
                    auto transformComponent = transformComponentPool.TryGet(entity);

                    if (!transformComponent)
                        return;

                    audioComponent.Update(*transformComponent, mainCamera.GetLocation());
                    });
            }

            // After everything we need to reset transform component changed and prepare the updated for next frame
            JobSystem::ParallelFor(jobGroup, int32_t(transformComponentPool.GetCount()), 8, [&](JobData&, int32_t idx) {
                auto& transformComponent = transformComponentPool.GetByIndex(idx);

                if (transformComponent.updated) {
                    transformComponent.changed = false;
                    transformComponent.updated = false;
                }
                });

            // After everything we need to reset transform component changed and prepare the updated for next frame
            JobSystem::ParallelFor(jobGroup, int32_t(hierarchyComponentPool.GetCount()), 4, [&](JobData&, int32_t idx) {
                auto& hierarchyComponent = hierarchyComponentPool.GetByIndex(idx);

                hierarchyComponent.updated = false;
                });

            renderState.CullAndSortLights();

            auto textSubset = entityManager.GetSubset<TextComponent, TransformComponent>();
            for (auto entity : textSubset) {
                const auto& [textComponent, transformComponent] = textSubset.Get(entity);

                textComponent.Update(transformComponent);
            }

            firstTimestep = false;

            JobSystem::Wait(jobGroup);

        }

        std::vector<ResourceHandle<Mesh::Mesh>> Scene::GetMeshes() {

            std::vector<ResourceHandle<Mesh::Mesh>> meshes;
            meshes.reserve(registeredMeshes.size());

            // Not really efficient, but does the job
            for (auto& [meshId, registeredMesh] : registeredMeshes) {
                meshes.push_back(registeredMesh.resource);
            }

            return meshes;

        }

        std::vector<ResourceHandle<Material>> Scene::GetMaterials() {

            std::vector<ResourceHandle<Material>> materials;

            // We could update the material usage bits in a separate method where we might not need 
            // any atomic operations. 
            if (terrain.IsLoaded()) {
                auto terrainMaterials = terrain->storage->GetMaterials();
                materials.reserve(terrainMaterials.size());

                for (const auto& material : terrainMaterials) {
                    if (!material.IsLoaded())
                        continue;

                    material->usage |= MaterialUsageBits::TerrainBit;
                    materials.push_back(material);
                }

            }

            auto meshes = GetMeshes();
            if (clutter) {
                auto vegMeshes = clutter->GetMeshes();
                meshes.insert(meshes.end(), vegMeshes.begin(), vegMeshes.end());
            }

            materials.reserve(materials.size() + meshes.size());

            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;
                for (auto& material : mesh->data.materials) {
                    if (!material.IsLoaded())
                        continue;

                    material->usage |= MaterialUsageBits::MeshBit;
                    materials.push_back(material);
                }
            }

            return materials;

        }

        CameraComponent& Scene::GetMainCamera() {

            return mainCameraEntity.GetComponent<CameraComponent>();

        }

        bool Scene::HasMainCamera() const {

            return mainCameraEntity.IsValid() && mainCameraEntity.HasComponent<CameraComponent>();

        }

        LightComponent& Scene::GetMainLight() {

            return mainLightEntity.GetComponent<LightComponent>();

        }

        bool Scene::HasMainLight() const {

            return mainLightEntity.IsValid() && mainLightEntity.HasComponent<LightComponent>();

        }

        Volume::RayResult<Entity> Scene::CastRay(Volume::Ray& ray, SceneQueryComponents queryComponents) {

            Volume::RayResult<Entity> result;
            result.hitDistance = ray.tMax;

            // Most accurate method if it works
            if (physicsWorld && (queryComponents & SceneQueryComponentBits::RigidBodyComponentBit)) {
                auto bodyResult = physicsWorld->CastRay(ray);

                if (bodyResult.valid) {
                    auto userData = bodyResult.data.GetUserData();

                    result.valid = true;
                    result.hitDistance = bodyResult.hitDistance;
                    result.normal = bodyResult.normal;
                    result.data = { userData, &entityManager };
                }
            }

            // This isn't really optimized, we could use hierarchical data structures
            if (queryComponents & SceneQueryComponentBits::MeshComponentBit) {
                auto meshSubset = entityManager.GetSubset<MeshComponent>();

                for (auto entity : meshSubset) {
                    const auto& meshComp = meshSubset.Get(entity);

                    auto dist = 0.0f;
                    ray.tMax = result.hitDistance;
                    if (ray.Intersects(meshComp.aabb, dist)) {
                        auto rigidBody = entityManager.TryGet<RigidBodyComponent>(entity);
                        // This means we already found a more accurate hit
                        if (result.valid && entityManager.Contains<RigidBodyComponent>(entity))
                            continue;

                        // Accept all hits greater equal if they were within the updated hit distance
                        if (dist > 0.0f) {
                            result.valid = true;
                            result.data = { entity, &entityManager };
                            result.hitDistance = dist;
                            result.normal = vec3(0.0f);
                        }

                        // Only accept zero hits (i.e we're inside their volume) if there wasn't anything before
                        if (!result.valid) {
                            result.valid = true;
                            result.data = { entity, &entityManager };
                            result.normal = vec3(0.0f);
                        }
                    }
                }
            }

            if (queryComponents & SceneQueryComponentBits::TextComponentBit) {
                auto textSubset = entityManager.GetSubset<TextComponent>();

                for (auto entity : textSubset) {
                    const auto& textComp = textSubset.Get(entity);

                    auto dist = 0.0f;
                    ray.tMax = result.hitDistance;
                    if (ray.Intersects(textComp.GetRectangle(), dist)) {

                        result.valid = true;
                        result.data = { entity, &entityManager };
                        result.hitDistance = dist;
                        result.normal = vec3(0.0f);
                    }
                }
            }

            if (queryComponents & SceneQueryComponentBits::TerrainComponentBit && terrain.IsLoaded()) {

                vec3 hitPosition;
                vec3 hitNormal;
                float hitDistance;
                if (terrain->IntersectRay(ray, hitPosition, hitNormal, hitDistance)) {

                    if (hitDistance < result.hitDistance) {
                        result.data = Entity();
                        result.valid = true;
                        result.hitDistance = hitDistance;
                        result.normal = hitNormal;
                    }
                }
            }

            return result;

        }

        std::vector<Entity> Scene::QueryAABB(const Volume::AABB& aabb, SceneQueryComponents queryComponents) {

            std::set<Entity> entities;

            if (queryComponents & SceneQueryComponentBits::MeshComponentBit) {

                auto queried = SpacePartitioning::QueryAABB(aabb);
                auto subset = entityManager.GetSubset<MeshComponent, TransformComponent>();
                for (auto entity : queried) {
                    auto meshComponent = entity.TryGetComponent<MeshComponent>();

                    bool validEntity = false;
                    if (meshComponent && meshComponent->aabb.Intersects(aabb))
                        validEntity = true;

                    if (validEntity)
                        entities.insert(entity);
                }

            }

            return { entities.begin(), entities.end() };

        }

        void Scene::GetRenderList(Volume::Frustum frustum, const Ref<RenderList::Pass>& pass) {

            if (!mainCameraEntity.IsValid())
                return;

            auto cameraPos = mainCameraEntity.GetComponent<CameraComponent>().GetLocation();

            bool mainPass = pass->type == RenderList::RenderPassType::Main;

            auto isDistCulled = [&](const MeshComponent& comp) -> bool {
                auto diff = comp.aabb.GetCenter() - cameraPos;

                float dist2 = glm::dot(diff, diff);
                float cullingDist = mainPass ? comp.mesh->distanceCulling : comp.mesh->shadowDistanceCulling;
                float cullingDist2 = cullingDist * cullingDist;

                return dist2 > cullingDist2;
                };

            // For the main pass we use the "dumb" method of just iterating over all the data, since we expect most things to
            // be visible and are exploiting the cache coherency in the meantime
            if (mainPass) {
                
                JobGroup jobGroup{ "Main pass culling", JobPriority::High};
                auto& meshComponentPool = entityManager.GetPool<MeshComponent>();
                JobSystem::ParallelFor(jobGroup, int32_t(meshComponentPool.GetCount()), 8, [&](JobData& data, int32_t idx) {
                    auto& comp = meshComponentPool.GetByIndex(idx);
                    auto entity = meshComponentPool[idx];

                    if (!comp.mesh.IsLoaded())
                        return;

                    if (comp.dontCull || comp.visible && !isDistCulled(comp) && frustum.Intersects(comp.aabb))
                        pass->Add(data.idx, entity, comp);
                    });

                JobSystem::Wait(jobGroup);
                pass->Finalize();

            }
            else {
                std::vector<Entity> entities;
                std::vector<Entity> insideEntities;
                renderableMovableEntityOctree.QueryFrustum(entities, insideEntities, frustum);

                auto& meshComponentPool = entityManager.GetPool<MeshComponent>();

                for (auto& entity : entities) {
                    auto& comp = meshComponentPool.Get(entity);
                    if (!comp.mesh.IsLoaded())
                        continue;

                    if (!comp.mesh->castShadow)
                        continue;

                    if (comp.dontCull || comp.visible && !isDistCulled(comp) && frustum.Intersects(comp.aabb))
                        pass->Add(entity, comp);
                }

                for (auto& entity : insideEntities) {
                    auto& comp = meshComponentPool.Get(entity);
                    if (!comp.mesh.IsLoaded())
                        continue;

                    if (!comp.mesh->castShadow)
                        continue;

                    if (comp.dontCull || comp.visible && !isDistCulled(comp))
                        pass->Add(entity, comp);
                }
            }

        }

        void Scene::ClearRTStructures() {

            WaitForAsyncWorkCompletion();

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

        void Scene::WaitForAsyncWorkCompletion() {

            renderState.WaitForAsyncWorkCompletion();

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

            registeredMeshes.clear();
            registeredAudios.clear();

        }

        SceneIterator Scene::begin() {

            return { &entityManager, entityManager.begin() };

        }

        SceneIterator Scene::end() {

            return { &entityManager, entityManager.end() };

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
                [this](const ECS::Entity entity, const MeshComponent& meshComponent) {
                    RegisterResource(registeredMeshes, meshComponent.mesh);
                });

            entityManager.SubscribeToTopic<MeshComponent>(ECS::Topic::ComponentErase,
                [this](const ECS::Entity entity, const MeshComponent& meshComponent) {
                    UnregisterResource(registeredMeshes, meshComponent.mesh);

                    if (meshComponent.inserted) {
                        SpacePartitioning::RemoveRenderableEntity(ToSceneEntity(entity), meshComponent);
                    }
                });

            // Need insert/remove physics components into physics world
            entityManager.SubscribeToTopic<RigidBodyComponent>(ECS::Topic::ComponentEmplace,
                [this](const ECS::Entity entity, RigidBodyComponent& rigidBodyComponent) {
                    auto transformComp = entityManager.TryGet<TransformComponent>(entity);
                    if (!transformComp) return;

                    if (physicsWorld != nullptr)
                        rigidBodyComponent.InsertIntoPhysicsWorld(*transformComp, physicsWorld.get());
                });

            entityManager.SubscribeToTopic<RigidBodyComponent>(ECS::Topic::ComponentErase,
                [this](const ECS::Entity entity, RigidBodyComponent& rigidBodyComponent) {
                    if (physicsWorld != nullptr)
                        rigidBodyComponent.RemoveFromPhysicsWorld();
                });

            entityManager.SubscribeToTopic<PlayerComponent>(ECS::Topic::ComponentEmplace,
                [this](const ECS::Entity entity, PlayerComponent& rigidBodyComponent) {
                    auto transformComp = entityManager.TryGet<TransformComponent>(entity);
                    if (!transformComp) return;

                    if (physicsWorld != nullptr)
                        rigidBodyComponent.InsertIntoPhysicsWorld(*transformComp, physicsWorld.get());
                });

            entityManager.SubscribeToTopic<PlayerComponent>(ECS::Topic::ComponentErase,
                [this](const ECS::Entity entity, PlayerComponent& rigidBodyComponent) {
                    if (physicsWorld != nullptr)
                        rigidBodyComponent.RemoveFromPhysicsWorld();
                });

        }

        std::unordered_map<ECS::Entity, Entity> Scene::Merge(const Ref<Scene>& other) {

            std::unordered_map<ECS::Entity, Entity> entityToEntityMap;

            // We need all entities before we can start to attach components
            for (auto entity : *other) {
                auto newEntity = CreateEntity();

                entityToEntityMap[entity] = newEntity;
            }

            for (auto entity : *other) {
                auto newEntity = entityToEntityMap[entity];

                DuplicateEntityComponents(entity, newEntity, &entityToEntityMap);
            }

            return entityToEntityMap;

        }

        void Scene::DuplicateEntityComponents(Entity srcEntity, Entity dstEntity, std::unordered_map<ECS::Entity, Entity>* mapper) {

            // This method is called both by the merge and the entity duplication methods
            // To handle both cases the only difference is the hierarchy component, where we either
            // need to map entities from the other scene in a merge or in case of the duplication need
            // to create new entities. 
            if (srcEntity.HasComponent<HierarchyComponent>()) {
                // This is not trivially copiable, need to preserve new entity relationships
                auto otherComp = srcEntity.GetComponent<HierarchyComponent>();
                auto& comp = dstEntity.AddComponent<HierarchyComponent>();

                comp.root = otherComp.root;

                // If we have a mapping table we can use that, otherwise create new entities
                if (mapper) {
                    for (auto compEntity : otherComp.entities) {
                        comp.AddChild((*mapper)[compEntity]);
                    }
                }
                else {
                    std::vector<Entity> childEntities;
                    childEntities.reserve(otherComp.GetChildren().size());

                    for (auto compEntity : otherComp.entities) {
                        auto newEntity = CreateEntity();

                        DuplicateEntityComponents(compEntity, newEntity, nullptr);

                        childEntities.push_back(newEntity);
                    }

                    // Comp might be dereferenced already
                    comp = dstEntity.GetComponent<HierarchyComponent>();
                    for (auto entity : childEntities)
                        comp.AddChild(entity);
                }
            }

            // NOTE: The reason we copy entities instead of taking a reference is simply because otherwise we might
            // get dereferenced due to new components being added, which could lead to crashes

            // Normal components without resources which are not a hierarchy that needs special treatment
            if (srcEntity.HasComponent<NameComponent>()) {
                const auto& otherComp = srcEntity.GetComponent<NameComponent>();
                dstEntity.AddComponent<NameComponent>(otherComp);
            }
            if (srcEntity.HasComponent<TransformComponent>()) {
                const auto& otherComp = srcEntity.GetComponent<TransformComponent>();
                dstEntity.AddComponent<TransformComponent>(otherComp);
            }
            if (srcEntity.HasComponent<CameraComponent>()) {
                const auto& otherComp = srcEntity.GetComponent<CameraComponent>();
                auto& comp = dstEntity.AddComponent<CameraComponent>(otherComp);
                // Set isMain to false by default
                comp.isMain = false;
            }
            if (srcEntity.HasComponent<LightComponent>()) {
                auto otherComp = srcEntity.GetComponent<LightComponent>();
                auto& comp = dstEntity.AddComponent<LightComponent>(otherComp);
                // Need to create a new shadow, since right now the memory is shared between components
                if (otherComp.shadow) {
                    comp.shadow = CreateRef<Lighting::Shadow>(*otherComp.shadow);
                    comp.shadow->SetResolution(comp.shadow->resolution);
                }
                comp.isMain = false;
            }
            if (srcEntity.HasComponent<RigidBodyComponent>()) {
                auto otherComp = srcEntity.GetComponent<RigidBodyComponent>();
                auto creationSettings = otherComp.GetBodyCreationSettings();
                const auto otherShape = creationSettings.shape;
                // Need to have a copy of the shape (otherwise they are all linked, e.g. when changing scale)
                creationSettings.shape = CreateRef<Physics::Shape>();
                *creationSettings.shape = *otherShape;
                dstEntity.AddComponent<RigidBodyComponent>(creationSettings);
            }
            if (srcEntity.HasComponent<PlayerComponent>()) {
                auto otherComp = srcEntity.GetComponent<PlayerComponent>();
                auto& comp = dstEntity.AddComponent<PlayerComponent>(otherComp);
                // Need to create a new creation settings, since right now the memory is shared between components
                comp.creationSettings = CreateRef<Physics::PlayerCreationSettings>(*otherComp.creationSettings);
            }

            // Resource components need extra attention (resources need to be registered in this scene)
            // We can do a straight copy afterwards, to get around copying every field
            if (srcEntity.HasComponent<MeshComponent>()) {
                auto otherComp = srcEntity.GetComponent<MeshComponent>();
                auto& comp = dstEntity.AddComponent<MeshComponent>(otherComp.mesh);
                comp = otherComp;
            }
            if (srcEntity.HasComponent<AudioComponent>()) {
                // These have a proper copy constructor
                auto otherComp = srcEntity.GetComponent<AudioComponent>();
                dstEntity.AddComponent<AudioComponent>(otherComp);
            }
            if (srcEntity.HasComponent<AudioVolumeComponent>()) {
                // These have a proper copy constructor
                auto otherComp = srcEntity.GetComponent<AudioVolumeComponent>();
                dstEntity.AddComponent<AudioVolumeComponent>(otherComp);
            }
            if (srcEntity.HasComponent<LuaScriptComponent>()) {
                // These have a proper copy constructor
                auto otherComp = srcEntity.GetComponent<LuaScriptComponent>();
                dstEntity.AddComponent<LuaScriptComponent>(otherComp);
            }
            if (srcEntity.HasComponent<TextComponent>()) {
                auto otherComp = srcEntity.GetComponent<TextComponent>();
                auto& comp = dstEntity.AddComponent<TextComponent>(otherComp.font, otherComp.text);
                comp = otherComp;
            }

        }

        void Scene::FindChangedTransforms(std::optional<std::function<void(void)>> waitFunction) {

            JobGroup jobGroup{ "Find changed transforms", JobPriority::High};

            auto& transformComponentPool = entityManager.GetPool<TransformComponent>();
            auto& hierarchyComponentPool = entityManager.GetPool<HierarchyComponent>();

            changedTransformSet.clear();
            for (auto& context : threadContexts) {
                context.changedTransforms.clear();
            }

            JobSystem::ParallelFor(jobGroup, int32_t(transformComponentPool.GetCount()), 8, [&](JobData& data, int32_t idx) {
                auto& transformComponent = transformComponentPool.GetByIndex(idx);
                if (!transformComponent.changed)
                    return;

                auto entity = transformComponentPool[idx];
                auto hierarchy = hierarchyComponentPool.TryGet(entity);

                int32_t level = hierarchy ? hierarchy->level : -1;

                threadContexts[data.idx].changedTransforms.push_back({ entity, level });
                });

            if (waitFunction.has_value())
                waitFunction.value()();

            JobSystem::Wait(jobGroup);

            for (const auto& context : threadContexts) {
                changedTransformSet.insert(changedTransformSet.end(), context.changedTransforms.begin(), context.changedTransforms.end());
            }

            std::sort(changedTransformSet.begin(), changedTransformSet.end(),
                [&](const auto& entity0, const auto& entity1) {
                    if (entity0.second == -1)
                        return false;

                    if (entity1.second == -1)
                        return true;

                    return entity0.second < entity1.second;
                });

        }

        void Scene::FindChangedMeshes(std::optional<std::function<void(void)>> waitFunction) {

            JobGroup jobGroup{ "Find changed meshes", JobPriority::High};

            changedMeshSet.clear();
            for (auto& context : threadContexts) {
                context.changedMeshes.clear();
            }

            auto& meshComponentPool = entityManager.GetPool<MeshComponent>();
            auto& transformComponentPool = entityManager.GetPool<TransformComponent>();

            JobSystem::ParallelFor(jobGroup, int32_t(meshComponentPool.GetCount()), 8, [&](JobData& data, int32_t idx) {
                auto entity = meshComponentPool[idx];
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!transformComponent)
                    return;

                auto& meshComponent = meshComponentPool.GetByIndex(idx);
                if (!meshComponent.mesh.IsLoaded()) {
                    // We can't update the transform yet
                    transformComponent->updated = false;
                    return;
                }

                if (!transformComponent->changed && meshComponent.inserted)
                    return;

                threadContexts[data.idx].changedMeshes.push_back(entity);
                });

            if (waitFunction.has_value())
                waitFunction.value()();

            JobSystem::Wait(jobGroup);

            for (const auto& context : threadContexts) {
                changedMeshSet.insert(changedMeshSet.end(), context.changedMeshes.begin(), context.changedMeshes.end());
            }

        }

        std::vector<uint8_t> Scene::Backup(const Ref<Scene>& scene) {

            json j;
            SceneToJson(j, scene.get());

            return json::to_msgpack(j);

        }

        Ref<Scene> Scene::Restore(const std::vector<uint8_t>& serialized) {

            json j = json::from_msgpack(serialized);

            Ref<Scene> scene;
            SceneFromJson(j, scene);

            return scene;

        }

    }

}
