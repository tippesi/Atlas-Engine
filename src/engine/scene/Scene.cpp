#include "Scene.h"
#include "Entity.h"
#include "SceneSerializer.h"
#include "components/Components.h"
#include "components/LuaScriptComponent.h"
#include "raytracing/RayTracingManager.h"

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

        mat4 Scene::GetGlobalMatrix(Entity entity) {

            if (!entity.IsValid())
                return mat4(1.0f);

            if (auto transformComponent = entity.TryGetComponent<TransformComponent>())
                return transformComponent->globalMatrix;

            if (auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>())
                return hierarchyComponent->globalMatrix;

            return mat4(1.0f);

        }

        void Scene::Update(float deltaTime, Entity mainCameraOverride) {

            AE_ASSERT((!mainCameraOverride.IsValid() || mainCameraOverride.HasComponent<CameraComponent>()) &&
                "Main camera override needs a camera component");

            this->deltaTime = paused ? 0.0f : deltaTime;
            time += this->deltaTime;

            WaitForAsyncWorkCompletion();

            // Do cleanup first such that we work with valid data
            CleanupUnusedResources();

            lastFrameDirtyEntities.swap(frameDirtyEntities);
            frameDirtyEntities.clear();

            // Motion vectors use the matrix of the last frame, SetGlobal before the update already stored it
            {
                auto& transformComponentPool = entityManager.GetPool<TransformComponent>();
                JobGroup historyJobGroup{ "Transform history", JobPriority::High };
                JobSystem::ParallelFor(historyJobGroup, int32_t(lastFrameDirtyEntities.size()), 64, [&](JobData&, int32_t idx) {
                    auto transformComponent = transformComponentPool.TryGet(lastFrameDirtyEntities[idx]);
                    if (transformComponent && !transformComponent->lastGlobalMatrixSet)
                        transformComponent->lastGlobalMatrix = transformComponent->globalMatrix;
                    });
                JobSystem::Wait(historyJobGroup);
            }

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
                        ocean->Update(mainCamera, this->deltaTime);
                        });
                }
            }

            auto rtDataNeeded = IsRtDataNeeded();
            if (rtDataNeeded)
                RayTracing::RayTracingManager::RequestMeshBVHs();

            // Kick of all the jobs while we find the changed transforms
            FindChangedTransforms([&]() {
                JobSystem::WaitSpin(terrainOceanJobGroup);

                // Can only update after scripts were runxx
                renderState.NewFrame();
                if (Graphics::GraphicsDevice::DefaultDevice->support.bindless) {
                    renderState.UpdateBlasBindlessData(rtDataNeeded);
                    renderState.UpdateOtherTextureBindlessData();
                }
                // Also updates the bindless textures of the materials
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
                    const auto& parentHierarchyComponent = hierarchyComponentPool.Get(parentEntity);
                    parentMatrix = parentHierarchyComponent.globalMatrix;
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
                    const auto& hierarchyComponent = hierarchyComponentPool.Get(parentEntity);
                    parentMatrix = hierarchyComponent.globalMatrix;
                }

                transformComponent.Update(parentMatrix, true);
                });

            JobSystem::Wait(jobGroup);

            // Wait for transform updates to finish
            if (physicsWorld != nullptr) {

                auto simulate = !physicsWorld->pauseSimulation && !paused;

                // This part (updating physics transforms) is required regardless of the simulation running
                auto playerSubset = entityManager.GetSubset<PlayerComponent, TransformComponent>();
                for (auto entity : playerSubset) {
                    const auto& [playerComponent, transformComponent] = playerSubset.Get(entity);

                    // Might happen if there was no transform at the creation of rigid body component
                    if (!playerComponent.IsValid()) {
                        playerComponent.InsertIntoPhysicsWorld(transformComponent, physicsWorld.get());
                        if (playerComponent.IsValid())
                            playerComponent.SetUserData(entity);
                    }

                    // Apply update here (transform overwrite everything else in physics simulation for now)
                    if (transformComponent.changed && playerComponent.IsValid()) {
                        auto& globalPlayerMatrix = transformComponent.globalMatrix;
                        auto decomposition = Common::MatrixDecomposition(globalPlayerMatrix);
                        // In theory we have to apply the rotation here as well
                        playerComponent.SetPosition(decomposition.translation);
                    }

                    if (simulate)
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

                    if (!rigidBodyComponent.IsValid())
                        continue;

                    // Kinematic bodies follow their transform through velocities, such that simulated bodies
                    // are pushed instead of penetrated. A teleport is still needed if no simulation step follows.
                    auto moveKinematic = rigidBodyComponent.layer == Physics::Layers::Kinematic &&
                        simulate && deltaTime > 0.0f;

                    // Apply update here (transform overwrite everything else in physics simulation for now)
                    if (transformComponent.changed) {
                        if (moveKinematic)
                            rigidBodyComponent.MoveKinematic(transformComponent.globalMatrix, deltaTime);
                        else
                            rigidBodyComponent.SetMatrix(transformComponent.globalMatrix);
                    }
                    else if (moveKinematic) {
                        // The velocity of the last move would otherwise carry the body away from its transform
                        physicsWorld->StopKinematicBody(rigidBodyComponent.bodyId);
                    }
                }

                // This part only needs to be executed if the simulation is running
                if (simulate) {

                    physicsWorld->Update(deltaTime);

                    for (auto entity : rigidBodySubset) {
                        const auto& [rigidBodyComponent, transformComponent] = rigidBodySubset.Get(entity);

                        // The transform drives kinematic bodies, reading them back would only drop the scale
                        if (!rigidBodyComponent.IsValid() || transformComponent.isStatic ||
                            rigidBodyComponent.layer == Physics::Layers::Static ||
                            rigidBodyComponent.layer == Physics::Layers::Kinematic)
                            continue;

                        // Bodies at rest are marked dirty every frame as well, could be skipped if the body isn't active

                        // This happens if no change was triggered by the user, then we still need
                        // to update the last global matrix, since it might have changed due to physics simulation
                        if (!transformComponent.changed) {
                            transformComponent.lastGlobalMatrix = transformComponent.globalMatrix;
                            MarkEntityDirty(ToSceneEntity(entity));
                        }

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
                        if (!transformComponent.changed) {
                            transformComponent.lastGlobalMatrix = transformComponent.globalMatrix;
                            MarkEntityDirty(ToSceneEntity(entity));
                        }

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
                mainCameraEntity = mainCameraOverride;
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

            // Clear old state
            mainLightEntity = Entity();

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
                JobSystem::Execute(renderState.rayTracingWorldUpdateJob, [this, rayTracingSubset, rtDataNeeded](JobData&) {
                    // Need to wait before updating graphic resources
                    Graphics::GraphicsDevice::DefaultDevice->WaitForPreviousFrameSubmission();
                    if (rayTracingWorld && rtDataNeeded) {
                        rayTracingWorld->scene = this;
                        // Don't update triangle lights for now (second argument)
                        rayTracingWorld->Update(rayTracingSubset, false);
                    }
                    // The bindless blas data isn't updated without any rt effects running
                    rtDataValid = rtDataNeeded && rayTracingWorld != nullptr && rayTracingWorld->IsValid();
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
            JobSystem::ParallelFor(jobGroup, int32_t(frameDirtyEntities.size()), 8, [&](JobData&, int32_t idx) {
                auto entity = frameDirtyEntities[idx];
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!transformComponent)
                    return;

                if (transformComponent->updated) {
                    transformComponent->changed = false;
                    transformComponent->updated = false;
                }
                transformComponent->lastGlobalMatrixSet = false;

                // Entities which couldn't be updated, e.g. because their mesh isn't loaded yet, stay dirty
                auto meshComponent = meshComponentPool.TryGet(entity);
                if (transformComponent->changed || (meshComponent && !meshComponent->inserted))
                    MarkEntityDirty(ToSceneEntity(entity));
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

            auto addMeshMaterials = [&](const ResourceHandle<Mesh::Mesh>& mesh) {
                if (!mesh.IsLoaded())
                    return;
                for (auto& material : mesh->data.materials) {
                    if (!material.IsLoaded())
                        continue;

                    material->usage |= MaterialUsageBits::MeshBit;
                    materials.push_back(material);
                }
                };

            // Iterate the registered meshes directly, copying all the mesh handles is expensive
            for (const auto& [meshId, registeredMesh] : registeredMeshes)
                addMeshMaterials(registeredMesh.resource);

            if (clutter) {
                for (const auto& mesh : clutter->GetMeshes())
                    addMeshMaterials(mesh);
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

        Volume::RayResult<Entity> Scene::CastRay(Volume::Ray& ray, SceneQueryComponents queryComponents,
            const std::function<bool(const Entity&)>& ignore) {

            Volume::RayResult<Entity> result;
            result.hitDistance = ray.tMax;

            auto isIgnored = [&](const Entity& entity) {
                return ignore && entity.IsValid() && ignore(entity);
            };

            if (physicsWorld && (queryComponents & SceneQueryComponentBits::RigidBodyComponentBit)) {
                auto bodyResult = physicsWorld->CastRay(ray, [&](const Physics::Body& body) {
                    return isIgnored({ body.GetUserData(), &entityManager });
                });

                if (bodyResult.valid) {
                    result.valid = true;
                    result.hitDistance = bodyResult.hitDistance;
                    result.normal = bodyResult.normal;
                    result.data = { bodyResult.data.GetUserData(), &entityManager };
                }
            }

            // Meshes with a mesh shape body of the same mesh were already hit exactly by the physics query above.
            if (queryComponents & SceneQueryComponentBits::MeshComponentBit) {
                auto queriedBodies = physicsWorld && (queryComponents & SceneQueryComponentBits::RigidBodyComponentBit);

                auto hasMeshBody = [&](ECS::Entity entity, const MeshComponent& meshComp) {
                    auto rigidBodyComp = entityManager.TryGet<RigidBodyComponent>(entity);
                    if (!rigidBodyComp || !rigidBodyComp->IsValid() || !rigidBodyComp->creationSettings)
                        return false;

                    const auto& shape = rigidBodyComp->creationSettings->shape;
                    if (!shape || shape->type != Physics::ShapeType::Mesh || !shape->settings)
                        return false;

                    auto meshSettings = static_cast<Physics::MeshShapeSettings*>(shape->settings.get());
                    return meshSettings->mesh == meshComp.mesh;
                };

                // Candidates are the meshes in the octree nodes along the ray, Update inserts them with their bounds
                std::vector<Entity> candidates;
                auto octreeRay = Volume::Ray(ray.origin, ray.direction, ray.tMin, result.hitDistance);
                renderableStaticEntityOctree.QueryRay(candidates, octreeRay);
                renderableMovableEntityOctree.QueryRay(candidates, octreeRay);

                // The bounds test comes first, most candidates are rejected by it
                for (auto sceneEntity : candidates) {
                    auto meshCompPtr = entityManager.TryGet<MeshComponent>(sceneEntity);
                    if (!meshCompPtr)
                        continue;

                    const auto& meshComp = *meshCompPtr;
                    auto boxRay = Volume::Ray(ray.origin, ray.direction, ray.tMin, result.hitDistance);
                    auto boxDistance = 0.0f;
                    if (!boxRay.Intersects(meshComp.aabb, boxDistance))
                        continue;

                    auto transformCompPtr = entityManager.TryGet<TransformComponent>(sceneEntity);
                    if (!transformCompPtr || !meshComp.mesh.IsLoaded() || (queriedBodies && hasMeshBody(sceneEntity, meshComp)))
                        continue;

                    const auto& transformComp = *transformCompPtr;

                    if (isIgnored(sceneEntity))
                        continue;

                    auto& data = meshComp.mesh->data;
                    auto& vertices = data.vertices.Get();
                    auto& indices = data.indices.Get();
                    if (vertices.empty() || indices.size() < 3)
                        continue;

                    // The ray parameter stays the same in local space as long as the direction isn't normalized
                    auto inverseMatrix = glm::inverse(transformComp.globalMatrix);
                    auto localRay = Volume::Ray(vec3(inverseMatrix * vec4(ray.origin, 1.0f)),
                        vec3(inverseMatrix * vec4(ray.direction, 0.0f)), ray.tMin, result.hitDistance);

                    int32_t hitTriangle = -1;
                    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                        auto t = 0.0f;
                        if (localRay.IntersectsTriangle(vertices[indices[i]], vertices[indices[i + 1]],
                            vertices[indices[i + 2]], t)) {
                            localRay.tMax = t;
                            hitTriangle = int32_t(i / 3);
                        }
                    }

                    if (hitTriangle < 0)
                        continue;

                    const auto& v0 = vertices[indices[hitTriangle * 3]];
                    const auto& v1 = vertices[indices[hitTriangle * 3 + 1]];
                    const auto& v2 = vertices[indices[hitTriangle * 3 + 2]];
                    auto normalMatrix = glm::transpose(glm::inverse(mat3(transformComp.globalMatrix)));
                    auto normal = glm::normalize(normalMatrix * glm::cross(v1 - v0, v2 - v0));

                    result.valid = true;
                    result.data = sceneEntity;
                    result.hitDistance = localRay.tMax;
                    // Report the side the ray hit
                    result.normal = glm::dot(normal, ray.direction) > 0.0f ? -normal : normal;
                    result.triangleIndex = hitTriangle;
                }
            }

            if (queryComponents & SceneQueryComponentBits::TextComponentBit) {
                auto textSubset = entityManager.GetSubset<TextComponent>();

                for (auto entity : textSubset) {
                    const auto& textComp = textSubset.Get(entity);

                    if (isIgnored({ entity, &entityManager }))
                        continue;

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

            auto isDone = [](const auto& handle) {
                return handle.IsLoaded() || (handle.IsValid() && handle.GetResource()->errorOnLoad);
            };

            bool loaded = true;

            auto meshes = GetMeshes();

            for (const auto& mesh : meshes) {
                loaded &= isDone(mesh);
            }

            if (terrain.IsValid())
                loaded &= isDone(terrain);

            return loaded;

        }

        bool Scene::IsRtDataValid() const {

            return rtDataValid;

        }

        bool Scene::IsRtDataNeeded() const {

            // Path tracing is checked by rayTracingWorld->includeObjectHistory
            return (irradianceVolume && irradianceVolume->enable) ||
                (reflection && reflection->enable && reflection->rt) ||
                (rayTracingWorld && rayTracingWorld->includeObjectHistory) ||
                (rtgi && rtgi->enable);

        }

        void Scene::Clear() {

            ClearRTStructures();
            entityManager.Clear();

            dirtyEntities.clear();
            frameDirtyEntities.clear();
            lastFrameDirtyEntities.clear();

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

            // New transforms and meshes need to be inserted into the space partitioning
            entityManager.SubscribeToTopic<TransformComponent>(ECS::Topic::ComponentEmplace,
                [this](const ECS::Entity entity, TransformComponent& transformComponent) {
                    transformComponent.changed = true;
                    MarkEntityDirty(ToSceneEntity(entity));
                });

            // Each resource type needs to count references
            entityManager.SubscribeToTopic<MeshComponent>(ECS::Topic::ComponentEmplace,
                [this](const ECS::Entity entity, const MeshComponent& meshComponent) {
                    RegisterResource(registeredMeshes, meshComponent.mesh);
                    MarkEntityDirty(ToSceneEntity(entity));
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
                    // Ray hits on the character body have to map back to the entity
                    if (rigidBodyComponent.IsValid())
                        rigidBodyComponent.SetUserData(entity);
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

            CollectDirtyEntities();

            JobSystem::ParallelFor(jobGroup, int32_t(frameDirtyEntities.size()), 8, [&](JobData& data, int32_t idx) {
                auto entity = frameDirtyEntities[idx];
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!transformComponent || !transformComponent->changed)
                    return;

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

            // Physics and hierarchy updates mark more entities dirty
            CollectDirtyEntities();

            JobSystem::ParallelFor(jobGroup, int32_t(frameDirtyEntities.size()), 8, [&](JobData& data, int32_t idx) {
                auto entity = frameDirtyEntities[idx];
                auto transformComponent = transformComponentPool.TryGet(entity);
                auto meshComponent = meshComponentPool.TryGet(entity);
                if (!transformComponent || !meshComponent)
                    return;

                if (!meshComponent->mesh.IsLoaded()) {
                    // We can't update the transform yet
                    transformComponent->updated = false;
                    return;
                }

                if (!transformComponent->changed && meshComponent->inserted)
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

        void Scene::MarkEntityDirty(const Entity& entity) {

            std::scoped_lock lock(dirtyEntitiesMutex);
            dirtyEntities.push_back(entity);

        }

        void Scene::CollectDirtyEntities() {

            {
                std::scoped_lock lock(dirtyEntitiesMutex);
                frameDirtyEntities.insert(frameDirtyEntities.end(), dirtyEntities.begin(), dirtyEntities.end());
                dirtyEntities.clear();
            }

            // An entity is marked again when its components are removed and added in the same frame
            std::sort(frameDirtyEntities.begin(), frameDirtyEntities.end());
            frameDirtyEntities.erase(std::unique(frameDirtyEntities.begin(), frameDirtyEntities.end()), frameDirtyEntities.end());

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
