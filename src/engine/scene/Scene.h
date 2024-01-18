#pragma once

#include "../System.h"
#include "../ecs/EntityManager.h"
#include "../resource/ResourceManager.h"

#include "../ocean/Ocean.h"
#include "../terrain/Terrain.h"
#include "../physics/PhysicsWorld.h"
#include "../lighting/Light.h"
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

#include "SpacePartitioning.h"
#include "Subset.h"

#include "components/Components.h"
#include "prefabs/Prefabs.h"

#include "RTData.h"
#include "Vegetation.h"

#include <type_traits>
#include <map>

namespace Atlas {

    namespace Scene {

        class Entity;

        class Scene : public SpacePartitioning {

            template<typename T>
            struct RegisteredResource {
                ResourceHandle<T> resource;
                uint32_t refCount = 0;
            };

        public:
            Scene() : SpacePartitioning(this, vec3(-2048.0f), vec3(2048.0f), 5) {};
            Scene(const Scene& that) = delete;
            explicit Scene(const std::string& name) 
                : name(name), SpacePartitioning(this, vec3(-2048.0f), vec3(2048.0f), 5) {}
            explicit Scene(const std::string& name, vec3 min, vec3 max, int32_t depth = 5) 
                : name(name), SpacePartitioning(this, min, max, depth) {}

            Entity CreateEntity();

            template<typename T, typename ...Args>
            T CreatePrefab(Args&&... args);

            void DestroyEntity(Entity entity);

            template<typename... Comp>
            Subset<Comp...> GetSubset();

            template<typename... Comp>
            void Merge(const Ref<Scene> other);

            void Update(Ref<Camera> camera, float deltaTime);

            void Update(float deltaTime);

            void UpdateCameraDependent(Ref<Camera> camera, float deltaTime);

            std::vector<ResourceHandle<Mesh::Mesh>> GetMeshes();

            std::vector<Material*> GetMaterials();

            void ClearRTStructures();

            void WaitForResourceLoad();

            bool IsFullyLoaded();

            bool IsRtDataValid();

            std::string name;

            Ref<Ocean::Ocean> ocean = nullptr;
            Ref<Terrain::Terrain> terrain = nullptr;
            Ref<Vegetation> vegetation = nullptr;
            Ref<Physics::PhysicsWorld> physicsWorld = nullptr;

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

            ECS::EntityManager entityManager = ECS::EntityManager(this);

            std::map<Hash, RegisteredResource<Mesh::Mesh>> registeredMeshes;

            std::unordered_map<Ref<Texture::Texture2D>, uint32_t> textureToBindlessIdx;
            std::unordered_map<size_t, uint32_t> meshIdToBindlessIdx;

            RTData rtData;

            bool hasChanged = true;
            bool rtDataValid = false;

            friend class Entity;
            friend class SceneSerializer;
            friend class SpacePartitioning;
            friend class RTData;
            friend class RenderList;
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

            return Subset<Comp...>(entityManager.GetSubset<Comp...>());

        }

        template<typename... Comp>
        void Scene::Merge(const Ref<Scene> other) {

            auto subset = GetSubset<Comp...>();

            for (auto entity : subset) {

                const auto components = subset.Get(entity);

                auto newEntity = CreateEntity();
                (newEntity.AddComponent(std::get<Comp&>(components)),...);

            }

        }

    }

}