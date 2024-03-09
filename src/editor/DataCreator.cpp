#include "DataCreator.h"

#include "loader/ModelLoader.h"

namespace Atlas::Editor {

    using namespace Scene::Components;
    using namespace Scene::Prefabs;

    Ref<Scene::Scene> DataCreator::CreateScene(const std::string& name, vec3 min, vec3 max, int32_t depth) {

        auto scene = CreateRef<Scene::Scene>(name, min, max, depth);

        auto mainGroup = scene->CreatePrefab<Group>("Root");
        auto& mainHierarchy = mainGroup.GetComponent<HierarchyComponent>();
        mainHierarchy.root = true;

        auto directionalLightEntity = scene->CreateEntity();
        directionalLightEntity.AddComponent<NameComponent>("Directional light");
        auto& directionalLight = directionalLightEntity.AddComponent<LightComponent>(LightType::DirectionalLight);

        directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 1.0f);
        directionalLight.color = glm::vec3(255, 236, 209) / 255.0f;
        directionalLight.intensity = 10.0f;
        directionalLight.AddDirectionalShadow(200.0f, 3.0f, 4096, glm::vec3(0.0f), vec4(-100.0f, 100.0f, -70.0f, 120.0f));
        directionalLight.isMain = true;

        mainHierarchy.AddChild(directionalLightEntity);

        scene->ao = CreateRef<Lighting::AO>(16);
        scene->ao->rt = true;
        // Use SSGI by default
        scene->ao->enable = false;
        scene->reflection = CreateRef<Lighting::Reflection>();
        scene->reflection->useShadowMap = true;

        scene->fog = CreateRef<Lighting::Fog>();
        scene->fog->enable = true;
        scene->fog->density = 0.0002f;
        scene->fog->heightFalloff = 0.0284f;
        scene->fog->height = 0.0f;

        scene->sky.atmosphere = CreateRef<Lighting::Atmosphere>();

        scene->postProcessing.taa = PostProcessing::TAA(0.99f);
        scene->postProcessing.sharpen.enable = true;
        scene->postProcessing.sharpen.factor = 0.15f;

        scene->irradianceVolume = CreateRef<Lighting::IrradianceVolume>(Volume::AABB(min, max), ivec3(20));

        scene->sss = CreateRef<Lighting::SSS>();

        scene->ssgi = CreateRef<Lighting::SSGI>();

        scene->sky.clouds = CreateRef<Lighting::VolumetricClouds>();
        scene->sky.clouds->minHeight = 1400.0f;
        scene->sky.clouds->maxHeight = 1700.0f;
        scene->sky.clouds->castShadow = false;

        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        return scene;

    }

    Ref<Scene::Scene> DataCreator::CreateSceneFromMesh(const std::string& filename, vec3 min, vec3 max,
        int32_t depth, bool invertUVs, bool addRigidBodies, bool combineMeshes, bool makeMeshesStatic) {

        auto scene = Loader::ModelLoader::LoadScene(filename, min, max, depth,
            combineMeshes, makeMeshesStatic, false, 2048);

        auto rootEntity = scene->GetEntityByName("Root");
        auto& mainHierarchy = rootEntity.GetComponent<HierarchyComponent>();

        auto directionalLightEntity = scene->CreateEntity();
        directionalLightEntity.AddComponent<NameComponent>("Directional light");
        auto& directionalLight = directionalLightEntity.AddComponent<LightComponent>(LightType::DirectionalLight);

        directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 1.0f);
        directionalLight.color = glm::vec3(255, 236, 209) / 255.0f;
        directionalLight.intensity = 10.0f;
        directionalLight.AddDirectionalShadow(200.0f, 3.0f, 4096, glm::vec3(0.0f), vec4(-100.0f, 100.0f, -70.0f, 120.0f));
        directionalLight.isMain = true;

        mainHierarchy.AddChild(directionalLightEntity);

        scene->ao = CreateRef<Lighting::AO>(16);
        scene->ao->rt = true;
        // Use SSGI by default
        scene->ao->enable = false;
        scene->reflection = CreateRef<Lighting::Reflection>();
        scene->reflection->useShadowMap = true;

        scene->fog = CreateRef<Lighting::Fog>();
        scene->fog->enable = true;
        scene->fog->density = 0.0002f;
        scene->fog->heightFalloff = 0.0284f;
        scene->fog->height = 0.0f;

        scene->sky.atmosphere = CreateRef<Lighting::Atmosphere>();

        scene->postProcessing.taa = PostProcessing::TAA(0.99f);
        scene->postProcessing.sharpen.enable = true;
        scene->postProcessing.sharpen.factor = 0.15f;

        scene->irradianceVolume = CreateRef<Lighting::IrradianceVolume>(Volume::AABB(min, max), ivec3(20));

        scene->sss = CreateRef<Lighting::SSS>();

        scene->ssgi = CreateRef<Lighting::SSGI>();

        scene->sky.clouds = CreateRef<Lighting::VolumetricClouds>();
        scene->sky.clouds->minHeight = 1400.0f;
        scene->sky.clouds->maxHeight = 1700.0f;
        scene->sky.clouds->castShadow = false;

        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        scene->Timestep(1.0f);

        if (invertUVs) {
            auto meshes = scene->GetMeshes();

            for (const auto& mesh : meshes)
                mesh->invertUVs = true;
        }

        if (addRigidBodies) {
            auto meshSubset = scene->GetSubset<MeshComponent>();

            for (auto entity : meshSubset) {
                const auto& meshComponent = meshSubset.Get(entity);

                auto transformComponent = entity.GetComponent<TransformComponent>();
                Atlas::Physics::MeshShapeSettings settings = {
                    .mesh = meshComponent.mesh,
                    .scale = transformComponent.DecomposeGlobal().scale
                };
                auto shape = Atlas::Physics::ShapesManager::CreateShape(settings);

                auto bodySettings = Atlas::Physics::BodyCreationSettings{
                    .objectLayer = Atlas::Physics::Layers::STATIC,
                    .shape = shape,
                };
                entity.AddComponent<RigidBodyComponent>(bodySettings);
            }
        }

        scene->physicsWorld->OptimizeBroadphase();

        return scene;

    }

}