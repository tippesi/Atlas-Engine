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
#include "../lighting/AO.h"
#include "../lighting/Reflection.h"
#include "../lighting/VolumetricClouds.h"
#include "../lighting/SSS.h"
#include "../lighting/SSGI.h"
#include "../postprocessing/PostProcessing.h"

#include "../mesh/Mesh.h"

#include "SceneIterator.h"
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

namespace Atlas {

    namespace Scene {

        typedef uint32_t SceneQueryComponents;

        typedef enum SceneQueryComponentBits {
            MeshComponentBit = (1 << 0),
            RigidBodyComponentBit = (1 << 1),
            TextComponentBit = (1 << 2),
            AllComponentsBit = (1 << 3) - 1
        } SceneQueryComponentBits;

        class Scene : public SpacePartitioning {

            template<typename T>
            struct RegisteredResource {
                ResourceHandle<T> resource;
                uint32_t refCount = 0;
            };

        public:
            Scene() : SpacePartitioning(this, vec3(-2048.0f), vec3(2048.0f), 5) { RegisterSubscribers(); }
            Scene(const Scene& that) = delete;
            explicit Scene(const std::string& name) : name(name),
                SpacePartitioning(this, vec3(-2048.0f), vec3(2048.0f), 5) { RegisterSubscribers(); }
            explicit Scene(const std::string& name, vec3 min, vec3 max, int32_t depth = 5) 
                : name(name), SpacePartitioning(this, min, max, depth) { RegisterSubscribers(); }

            ~Scene();

            Entity CreateEntity();

            template<typename T, typename ...Args>
            T CreatePrefab(Args&&... args);

            void DestroyEntity(Entity entity, bool removeRecursively = true);

            Entity DuplicateEntity(Entity entity);

            size_t GetEntityCount() const;

            Entity GetEntityByName(const std::string& name);

            Entity GetParentEntity(Entity entity);

            template<typename... Comp>
            Subset<Comp...> GetSubset();

            std::unordered_map<ECS::Entity, Entity> Merge(const Ref<Scene>& other);

            void Timestep(float deltaTime);

            void Update();

            std::vector<ResourceHandle<Mesh::Mesh>> GetMeshes();

            std::vector<Ref<Material>> GetMaterials();

            CameraComponent& GetMainCamera();

            bool HasMainCamera() const;

            Volume::RayResult<Entity> CastRay(Volume::Ray& ray, 
                SceneQueryComponents queryComponents = SceneQueryComponentBits::AllComponentsBit);

            void GetRenderList(Volume::Frustum frustum, RenderList& renderList, const Ref<RenderList::Pass>& pass);

            void ClearRTStructures();

            void WaitForResourceLoad();

            void WaitForAsyncWorkCompletion();

            bool IsFullyLoaded();

            bool IsRtDataValid() const;

            void Clear();

            SceneIterator begin();

            SceneIterator end();

            static std::vector<uint8_t> Backup(const Ref<Scene>& scene);

            static Ref<Scene> Restore(const std::vector<uint8_t>& serialized);

            std::string name;

            Ref<Ocean::Ocean> ocean = nullptr;
            Ref<Terrain::Terrain> terrain = nullptr;
            Ref<Clutter> clutter = nullptr;
            Ref<Physics::PhysicsWorld> physicsWorld = nullptr;
            Ref<RayTracing::RayTracingWorld> rayTracingWorld = nullptr;            

            Wind wind;
            Lighting::Sky sky;
            Ref<Lighting::Fog> fog = nullptr;
            Ref<Lighting::IrradianceVolume> irradianceVolume = nullptr;
            Ref<Lighting::AO> ao = nullptr;
            Ref<Lighting::Reflection> reflection = nullptr;
            Ref<Lighting::SSS> sss = nullptr;
            Ref<Lighting::SSGI> ssgi = nullptr;
            PostProcessing::PostProcessing postProcessing;

        private:
            void UpdateBindlessIndexMaps();

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

            ECS::EntityManager entityManager = ECS::EntityManager(this);

            std::unordered_map<ECS::Entity, ECS::Entity> childToParentMap;
            std::map<Hash, RegisteredResource<Mesh::Mesh>> registeredMeshes;
            std::map<Hash, RegisteredResource<Audio::AudioData>> registeredAudios;

            std::unordered_map<Ref<Texture::Texture2D>, uint32_t> textureToBindlessIdx;
            std::unordered_map<size_t, uint32_t> meshIdToBindlessIdx;

            Entity mainCameraEntity;
            float deltaTime = 1.0f;

            bool firstTimestep = true;
            bool hasChanged = true;
            bool rtDataValid = false;
            bool vegetationChanged = false;

            Scripting::LuaScriptManager luaScriptManager = Scripting::LuaScriptManager(this);

            std::future<void> rayTracingWorldUpdateFuture;

            friend Entity;
            friend SpacePartitioning;
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