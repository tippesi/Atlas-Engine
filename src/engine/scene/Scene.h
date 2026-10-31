#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"
#include "../resource/ResourceManager.h"

#include "../ocean/Ocean.h"
#include "../terrain/Terrain.h"
#include "../physics/PhysicsWorld.h"
#include "../lighting/Sky.h"
#include "../lighting/Fog.h"
#include "../lighting/IrradianceVolume.h"
#include "../lighting/RTGI.h"
#include "../lighting/SSGI.h"
#include "../lighting/Reflection.h"
#include "../lighting/VolumetricClouds.h"
#include "../lighting/SSS.h"
#include "../postprocessing/PostProcessing.h"

#include "../mesh/Mesh.h"

#include "SceneIterator.h"
#include "SceneRenderState.h"
#include "SpacePartitioning.h"
#include "Subset.h"
#include "Wind.h"

#include "components/Components.h"
#include "prefabs/Prefabs.h"

#include "raytracing/RayTracingWorld.h"
#include "Clutter.h"

#include "../scripting/LuaScriptManager.h"

#include <type_traits>
#include <map>
#include <functional>
#include <mutex>
#include <optional>

namespace Atlas {

    namespace Scene {

        typedef uint32_t SceneQueryComponents;

        typedef enum SceneQueryComponentBits {
            MeshComponentBit = (1 << 0),
            RigidBodyComponentBit = (1 << 1),
            TextComponentBit = (1 << 2),
            TerrainComponentBit = (1 << 3),
            AllComponentsBit = (1 << 4) - 1
        } SceneQueryComponentBits;

        // Seen from the entity: both entities of a contact get an event
        struct ContactEvent {
            Physics::ContactEventType type = Physics::ContactEventType::Begin;

            Entity entity;
            Entity otherEntity;

            // Contact position and normal pointing from the entity to the other entity, both are zero for end events
            vec3 position = vec3(0.0f);
            vec3 normal = vec3(0.0f);

            float impactSpeed = 0.0f;

            bool isTrigger = false;
        };

        class Scene : public SpacePartitioning {

            template<typename T>
            struct RegisteredResource {
                ResourceHandle<T> resource;
                uint32_t refCount = 0;
            };

        public:
            Scene() : SpacePartitioning(this, vec3(-2048.0f), vec3(2048.0f), 6), renderState(this)
                { RegisterSubscribers(); }
            Scene(const Scene& that) = delete;
            explicit Scene(const std::string& name) : SpacePartitioning(this, vec3(-2048.0f), vec3(2048.0f), 6),
                name(name), renderState(this) { RegisterSubscribers(); }
            explicit Scene(const std::string& name, vec3 min, vec3 max, int32_t depth = 6) 
                : SpacePartitioning(this, min, max, depth), name(name), renderState(this) { RegisterSubscribers(); }

            ~Scene();

            Entity CreateEntity();

            template<typename T, typename ...Args>
            T CreatePrefab(Args&&... args);

            void DestroyEntity(Entity entity, bool removeRecursively = true, 
                bool parentDeleted = false);

            // The duplicate (with copies of all descendants) has the same parent as the entity
            Entity DuplicateEntity(Entity entity);

            size_t GetEntityCount() const;

            Entity GetEntityByName(const std::string& name);

            Entity GetParentEntity(Entity entity);

            // The entity whose hierarchy is marked as root, invalid if the scene has none
            Entity GetRootEntity();

            // World matrix of the transform, entities without one use the matrix of their hierarchy
            mat4 GetGlobalMatrix(Entity entity);

            // Union of the mesh bounds of the entity and its descendants, empty if none of them has a loaded mesh
            std::optional<Volume::AABB> GetHierarchyBounds(Entity entity);

            template<typename... Comp>
            Subset<Comp...> GetSubset();

            template<typename Comp>
            size_t GetComponentCount();

            std::unordered_map<ECS::Entity, Entity> Merge(const Ref<Scene>& other);

            // A valid mainCameraOverride is used as the main camera regardless of the isMain flags
            void Update(float deltaTime, Entity mainCameraOverride = Entity());

            /**
             * Time in seconds that the scene was updated for. It stands still while the scene is paused,
             * everything animated by the renderers (wind, clouds, water) uses it.
             */
            float GetTime() const { return time; }

            /**
             * Time step of the last update in seconds, 0 while the scene is paused.
             */
            float GetDeltaTime() const { return deltaTime; }

            std::vector<ResourceHandle<Mesh::Mesh>> GetMeshes();

            std::vector<ResourceHandle<Material>> GetMaterials();

            CameraComponent& GetMainCamera();

            bool HasMainCamera() const;

            LightComponent& GetMainLight();

            /**
             * Plays audio at a location without an entity, heard from the main camera. The sound pauses
             * with the scene and is removed once it played to its end.
             */
            Ref<Audio::AudioStream> PlayAudio(const ResourceHandle<Audio::AudioData>& data, vec3 location,
                float volume = 1.0f, float falloffFactor = 0.5f, float falloffPower = 2.0f);

            bool HasMainLight() const;

            /**
             * Casts a ray against the scene. Physics bodies are tested with their collision shapes, meshes with
             * triangle precision. If bodies are queried, meshes covered by a mesh shape body are only hit
             * through the physics world instead of being tested twice. Only mesh hits report a triangle
             * index, query meshes without bodies to get the visible triangle. Entities for which ignore returns true are skipped.
             */
            Volume::RayResult<Entity> CastRay(Volume::Ray& ray,
                SceneQueryComponents queryComponents = SceneQueryComponentBits::AllComponentsBit,
                const std::function<bool(const Entity&)>& ignore = nullptr);

            std::vector<Entity> QueryAABB(const Volume::AABB& aabb, 
                SceneQueryComponents queryComponents = SceneQueryComponentBits::AllComponentsBit);

            /**
             * Moves the shape from the matrix along the direction and returns the first physics body it hits.
             * The hit distance is measured along the direction.
             */
            Volume::RayResult<Entity> CastShape(const Ref<Physics::Shape>& shape, const mat4& matrix, vec3 direction,
                float distance, const std::function<bool(const Entity&)>& ignore = nullptr);

            std::vector<Entity> CollideShape(const Ref<Physics::Shape>& shape, const mat4& matrix,
                const std::function<bool(const Entity&)>& ignore = nullptr);

            /**
             * Contacts of physics bodies and players that began or ended in the last update. Scripts get them before
             * their update through OnContactBegin, OnContactEnd, OnTriggerEnter and OnTriggerExit.
             */
            const std::vector<ContactEvent>& GetContactEvents() const { return contactEvents; }

            void GetRenderList(Volume::Frustum frustum, const Ref<RenderList::Pass>& pass);

            void ClearRTStructures();

            void WaitForResourceLoad();

            void WaitForAsyncWorkCompletion();

            // Meshes and terrain finished loading, resources that failed to load count as finished
            bool IsFullyLoaded();

            bool IsRtDataValid() const;

            bool IsRtDataNeeded() const;

            void Clear();

            SceneIterator begin();

            SceneIterator end();

            static std::vector<uint8_t> Backup(const Ref<Scene>& scene);

            static Ref<Scene> Restore(const std::vector<uint8_t>& serialized);

            std::string name;

            // Halts scripts, physics and players of a running simulation while keeping their state
            bool paused = false;

            Ref<Clutter> clutter = nullptr;
            Ref<Physics::PhysicsWorld> physicsWorld = nullptr;
            Ref<RayTracing::RayTracingWorld> rayTracingWorld = nullptr;

            Ref<Ocean::Ocean> ocean = nullptr;
            ResourceHandle<Terrain::Terrain> terrain;       

            Wind wind;
            Lighting::Sky sky;
            Ref<Lighting::Fog> fog = nullptr;
            Ref<Lighting::IrradianceVolume> irradianceVolume = nullptr;
            Ref<Lighting::RTGI> rtgi = nullptr;
            Ref<Lighting::SSGI> ssgi = nullptr;
            Ref<Lighting::Reflection> reflection = nullptr;
            Ref<Lighting::SSS> sss = nullptr;
            PostProcessing::PostProcessing postProcessing;

            SceneRenderState renderState;
            ECS::EntityManager entityManager = ECS::EntityManager(this);

        protected:
            void MarkEntityDirty(const Entity& entity);

        private:
            struct ThreadContext {
                std::vector<std::pair<ECS::Entity, int32_t>> changedTransforms;
                std::vector<ECS::Entity> changedMeshes;
            }threadContexts[8];
            
            Entity ToSceneEntity(ECS::Entity entity);

            void RegisterSubscribers();

            void CleanupUnusedResources();

            void DuplicateEntityComponents(Entity srcEntity, Entity dstEntity, std::unordered_map<ECS::Entity, Entity>* mapper);

            template<class T>
            void RegisterResource(std::map<Hash, RegisteredResource<T>>& resources, ResourceHandle<T> resource);

            template<class T>
            void UnregisterResource(std::map<Hash, RegisteredResource<T>>& resources, ResourceHandle<T> resource);

            template<class T>
            void CleanupUnusedResources(std::map<Hash, RegisteredResource<T>>& registeredResources);           

            void FindChangedTransforms(std::optional<std::function<void(void)>> waitFunction = std::nullopt);

            void FindChangedMeshes(std::optional<std::function<void(void)>> waitFunction = std::nullopt);

            void CollectDirtyEntities();

            std::unordered_map<ECS::Entity, ECS::Entity> childToParentMap;
            std::map<Hash, RegisteredResource<Mesh::Mesh>> registeredMeshes;
            std::map<Hash, RegisteredResource<Audio::AudioData>> registeredAudios;

            struct PositionalAudio {
                Ref<Audio::AudioStream> stream;
                vec3 location;
                float volume;
                float falloffFactor;
                float falloffPower;
            };

            std::vector<PositionalAudio> positionalAudios;
            std::mutex positionalAudioMutex;

            Entity mainCameraEntity;
            Entity mainLightEntity;
            float time = 0.0f;
            float deltaTime = 0.0f;

            std::vector<ContactEvent> contactEvents;

            bool firstTimestep = true;
            bool hasChanged = true;
            bool rtDataValid = false;
            bool vegetationChanged = false;

            std::vector<std::pair<ECS::Entity, int32_t>> changedTransformSet;
            std::vector<ECS::Entity> changedMeshSet;

            std::mutex dirtyEntitiesMutex;
            std::vector<ECS::Entity> dirtyEntities;
            std::vector<ECS::Entity> frameDirtyEntities;
            std::vector<ECS::Entity> lastFrameDirtyEntities;

            Scripting::LuaScriptManager luaScriptManager = Scripting::LuaScriptManager(this);

            friend Entity;
            friend SpacePartitioning;
            friend SceneRenderState;
            friend RayTracing::RayTracingWorld;
            friend HierarchyComponent;
            friend MeshComponent;
            friend AudioVolumeComponent;
            friend AudioComponent;
            friend RenderList;
            friend class SceneSerializer;
            friend class Renderer::Helper::RayTracingHelper;
            friend class Renderer::MainRenderer;

        };

        template<typename T, typename ...Args>
        T Scene::CreatePrefab(Args&&... args) {

            static_assert(std::is_convertible<T, Entity>() && sizeof(T) == (sizeof(Scene*) + sizeof(ECS::Entity)),
                "Prefab needs to inherit from Scene::Entity class without any extra members");
            static_assert(std::is_constructible<T, ECS::Entity, ECS::EntityManager*, Args...>(),
                "Can't construct prefab with given arguments. Prefab needs to have at \
                least ECS::Entity and Scene* as constructor arguments.");

            return T(entityManager.Create(), &entityManager, std::forward<Args>(args)...);

        }

        template<typename... Comp>
        Subset<Comp...> Scene::GetSubset() {

            return Subset<Comp...>(entityManager.GetSubset<Comp...>(), &entityManager);

        }

        template<typename Comp>
        size_t Scene::GetComponentCount() {

            return entityManager.GetCount<Comp>();

        }

        template<class T>
        void Scene::RegisterResource(std::map<Hash, RegisteredResource<T>>& resources, ResourceHandle<T> resource) {

            if (!resources.contains(resource.GetID()))
                resources[resource.GetID()] = { .resource = resource, .refCount = 1 };
            else
                resources[resource.GetID()].refCount++;

        }

        template<class T>
        void Scene::UnregisterResource(std::map<Hash, RegisteredResource<T>>& resources, ResourceHandle<T> resource) {

            if (resources.contains(resource.GetID()))
                resources[resource.GetID()].refCount--;

        }

        template<typename T>
        void Scene::CleanupUnusedResources(std::map<Hash, RegisteredResource<T>> &registeredResources) {
            std::vector<Hash> toBeDeleted;

            for (const auto &[hash, resource] : registeredResources) {
                if (resource.refCount == 0)
                    toBeDeleted.push_back(hash);
            }

            for (const auto hash : toBeDeleted) {
                registeredResources.erase(hash);
            }
        }

    }

}