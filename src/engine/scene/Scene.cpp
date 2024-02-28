#include "Scene.h"
#include "Entity.h"
#include "SceneSerializer.h"
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

        void Scene::DestroyEntity(Entity entity, bool removeRecursively) {

            auto parentEntity = GetParentEntity(entity);

            if (parentEntity.IsValid()) {
                auto& hierarchyComponent = parentEntity.GetComponent<HierarchyComponent>();
                hierarchyComponent.RemoveChild(entity);
            }

            if (removeRecursively) {
                auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
                if (hierarchyComponent) {
                    auto children = hierarchyComponent->GetChildren();
                    for (auto childEntity : children) {
                        DestroyEntity(childEntity, removeRecursively);
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

        void Scene::Timestep(float deltaTime) {

            this->deltaTime = deltaTime;

            // Do cleanup first such that we work with valid data
            CleanupUnusedResources();

            // Update scripting components
            auto luaScriptComponentSubset = entityManager.GetSubset<LuaScriptComponent>();
            for (auto entity : luaScriptComponentSubset) {
                auto& luaScriptComponent = luaScriptComponentSubset.Get<LuaScriptComponent>(entity);
                luaScriptComponent.Update(luaScriptManager, deltaTime);
            }

            TransformComponent rootTransform = {};

            auto hierarchyTransformSubset = entityManager.GetSubset<HierarchyComponent, TransformComponent>();
            // Update hierarchy and their entities
            for (auto entity : hierarchyTransformSubset) {
                const auto& [hierarchyComponent, transformComponent] = hierarchyTransformSubset.Get(entity);

                if (hierarchyComponent.root) {
                    auto parentChanged = transformComponent.changed;
                    transformComponent.Update(rootTransform, false);
                    hierarchyComponent.Update(transformComponent, parentChanged);
                }
            }

            // Update hierarchy components which are not part of a root hierarchy that also has a transform component
            // This might be the case if there is an entity that has just a hierarchy without a tranform for, e.g. grouping entities
            for (auto entity : hierarchyTransformSubset) {
                const auto& [hierarchyComponent, transformComponent] = hierarchyTransformSubset.Get(entity);

                if (!hierarchyComponent.updated) {
                    auto parentChanged = transformComponent.changed;
                    transformComponent.Update(rootTransform, false);
                    hierarchyComponent.Update(transformComponent, parentChanged);
                }
            }

            auto transformSubset = entityManager.GetSubset<TransformComponent>();

            // Update all other transforms not affected by the hierarchy (entities don't need to be in hierarchy)
            for (auto entity : transformSubset) {
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);

                if (!transformComponent.updated) {
                    transformComponent.Update(rootTransform, false);
                }
            }

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
                            rigidBodyComponent.layer == Physics::Layers::STATIC)
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
                }
            }

            // Do the space partitioning update here (ofc also update AABBs)
            auto meshSubset = entityManager.GetSubset<MeshComponent, TransformComponent>();
            for (auto entity : meshSubset) {
                auto& meshComponent = entityManager.Get<MeshComponent>(entity);
                auto& transformComponent = entityManager.Get<TransformComponent>(entity);
                if (!meshComponent.mesh.IsLoaded()) {
                    // We can't update the transform yet
                    transformComponent.updated = false;
                    continue;
                }

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

                if (transformComponent.updated) {
                    transformComponent.changed = false;
                    transformComponent.updated = false;
                }
            }

            // We also need to reset the hierarchy components as well
            auto hierarchySubset = entityManager.GetSubset<HierarchyComponent>();
            for (auto entity : hierarchySubset) {
                auto& hierarchyComponent = hierarchySubset.Get(entity);

                hierarchyComponent.updated = false;
            }

            // Everything below assumes that entities themselves have a transform
            // Without it they won't be transformed when they are in a hierarchy
            auto cameraSubset = entityManager.GetSubset<CameraComponent, TransformComponent>();
            for (auto entity : cameraSubset) {
                const auto& [cameraComponent, transformComponent] = cameraSubset.Get(entity);

                cameraComponent.parentTransform = transformComponent.globalMatrix;
            }

            auto textSubset = entityManager.GetSubset<TextComponent, TransformComponent>();
            for (auto entity : textSubset) {
                const auto& [textComponent, transformComponent] = textSubset.Get(entity);

                textComponent.Update(transformComponent);
            }

            auto lightSubset = entityManager.GetSubset<LightComponent>();
            for (auto entity : lightSubset) {
                auto& lightComponent = lightSubset.Get(entity);

                auto transformComponent = entityManager.TryGet<TransformComponent>(entity);

                lightComponent.Update(transformComponent);
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

        void Scene::Update() {

            mainCameraEntity = Entity();

            auto cameraSubset = entityManager.GetSubset<CameraComponent>();

            // Attempt to find a main camera
            for (auto entity : cameraSubset) {
                auto& camera = cameraSubset.Get(entity);

                mat4 transformMatrix = mat4(1.0f);
                auto transform = entityManager.TryGet<TransformComponent>(entity);
                if (transform) {
                    transformMatrix = transform->globalMatrix;
                }

                camera.Update(transformMatrix);

                if (camera.isMain && !mainCameraEntity.IsValid()) {
                    mainCameraEntity = { entity, &entityManager };
                }
            }

            AE_ASSERT(mainCameraEntity.IsValid() && "Couldn't find main camera component");

            if (!mainCameraEntity.IsValid())
                return;

            auto& mainCamera = mainCameraEntity.GetComponent<CameraComponent>();

            auto audioSubset = entityManager.GetSubset<AudioComponent, TransformComponent>();
            for (auto entity : audioSubset) {
                const auto& [audioComponent, transformComponent] = audioSubset.Get(entity);

                audioComponent.Update(deltaTime, transformComponent, mainCamera.GetLocation(),
                    mainCamera.GetLastLocation(), mainCamera.right);
            }

            auto audioVolumeSubset = entityManager.GetSubset<AudioVolumeComponent, TransformComponent>();
            for (auto entity : audioVolumeSubset) {
                const auto& [audioComponent, transformComponent] = audioVolumeSubset.Get(entity);

                audioComponent.Update(transformComponent, mainCamera.GetLocation());
            }

            auto lightSubset = entityManager.GetSubset<LightComponent>();
            for (auto entity : lightSubset) {
                auto& lightComponent = lightSubset.Get(entity);

                lightComponent.Update(mainCamera);
            }

            if (terrain) {
                terrain->Update(mainCamera);
            }

            if (ocean) {
                ocean->Update(mainCamera, deltaTime);
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

        std::vector<Ref<Material>> Scene::GetMaterials() {

            std::vector<Ref<Material>> materials;

            if (terrain) {
                auto terrainMaterials = terrain->storage.GetMaterials();

                for (const auto& material : terrainMaterials) {
                    if (!material)
                        continue;

                    materials.push_back(material);
                }

            }

            auto meshes = GetMeshes();
            if (clutter) {
                auto vegMeshes = clutter->GetMeshes();
                meshes.insert(meshes.end(), vegMeshes.begin(), vegMeshes.end());
            }

            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;
                for (auto& material : mesh->data.materials) {
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

        Volume::RayResult<Entity> Scene::CastRay(Volume::Ray& ray, SceneQueryComponents queryComponents) {

            Volume::RayResult<Entity> result;

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
                    if (ray.Intersects(meshComp.aabb, 0.0f, result.hitDistance, dist)) {
                        auto rigidBody = entityManager.TryGet<RigidBodyComponent>(entity);
                        // This means we already found a more accurate hit
                        if (result.valid && entityManager.Contains<RigidBodyComponent>(entity))
                            continue;

                        // Accept all hits greater equal if they were within the updated hit distance
                        if (dist > 0.0f) {
                            result.valid = true;
                            result.data = { entity, &entityManager };
                            result.hitDistance = dist;
                        }

                        // Only accept zero hits (i.e we're inside their volume) if there wasn't anything before
                        if (!result.valid) {
                            result.valid = true;
                            result.data = { entity, &entityManager };
                        }
                    }
                }
            }

            if (queryComponents & SceneQueryComponentBits::TextComponentBit) {
                auto textSubset = entityManager.GetSubset<TextComponent>();

                for (auto entity : textSubset) {
                    const auto& textComp = textSubset.Get(entity);

                    auto dist = 0.0f;
                    if (ray.Intersects(textComp.GetRectangle(), 0.0f, result.hitDistance, dist)) {

                        result.valid = true;
                        result.data = { entity, &entityManager };
                        result.hitDistance = dist;

                    }
                }
            }

            return result;

        }

        void Scene::GetRenderList(Volume::Frustum frustum, Atlas::RenderList& renderList) {

            // This is much quicker presumably due to cache coherency (need better hierarchical data structure)
            auto subset = entityManager.GetSubset<MeshComponent, TransformComponent>();
            for (auto& entity : subset) {
                auto& comp = subset.Get<MeshComponent>(entity);

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

            registeredMeshes.clear();
            registeredAudios.clear();

        }

        SceneIterator Scene::begin() {

            return { &entityManager, entityManager.begin() };

        }

        SceneIterator Scene::end() {

            return { &entityManager, entityManager.end() };

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

            auto lightSubset = entityManager.GetSubset<LightComponent>();
            for (auto entity : lightSubset) {
                const auto& lightComponent = lightSubset.Get(entity);

                if (!lightComponent.shadow)
                    continue;

                if (lightComponent.shadow->useCubemap) {

                }
                else {

                }
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
                comp.shadow = CreateRef<Lighting::Shadow>(*otherComp.shadow);
                comp.shadow->SetResolution(comp.shadow->resolution);
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

        std::vector<uint8_t> Scene::Backup(const Ref<Scene>& scene) {

            json j;
            SceneToJson(j, scene.get());

            return json::to_bjdata(j);

        }

        Ref<Scene> Scene::Restore(const std::vector<uint8_t>& serialized) {

            json j = json::from_bjdata(serialized);

            Ref<Scene> scene;
            SceneFromJson(j, scene);

            return scene;

        }

    }

}