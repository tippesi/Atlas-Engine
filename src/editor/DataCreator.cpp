#include "DataCreator.h"

namespace Atlas::Editor {

    using namespace Scene::Components;
    using namespace Scene::Prefabs;

    Ref<Scene::Scene> DataCreator::CreateScene(const std::string &name, vec3 min, vec3 max, int32_t depth) {

        auto scene = Atlas::CreateRef<Scene::Scene>(name, min, max, depth);

        auto mainGroup = scene->CreatePrefab<Group>("Root");
        auto& mainHierarchy = mainGroup.GetComponent<HierarchyComponent>();

        auto directionalLightEntity = scene->CreateEntity();
        directionalLightEntity.AddComponent<NameComponent>("Directional light");
        auto& directionalLight = directionalLightEntity.AddComponent<LightComponent>(LightType::DirectionalLight);

        directionalLight.properties.directional.direction = glm::vec3(0.0f, -1.0f, 1.0f);
        directionalLight.color = glm::vec3(255, 236, 209) / 255.0f;
        directionalLight.intensity = 10.0f;
        glm::mat4 orthoProjection = glm::ortho(-100.0f, 100.0f, -70.0f, 120.0f, -120.0f, 120.0f);
        directionalLight.AddDirectionalShadow(200.0f, 3.0f, 4096, glm::vec3(0.0f), orthoProjection);
        directionalLight.isMain = true;

        mainHierarchy.entities.push_back(directionalLightEntity);

        scene->ao = Atlas::CreateRef<Atlas::Lighting::AO>(16);
        scene->ao->rt = true;
        // Use SSGI by default
        scene->ao->enable = false;
        scene->reflection = Atlas::CreateRef<Atlas::Lighting::Reflection>();
        scene->reflection->useShadowMap = true;

        scene->fog = Atlas::CreateRef<Atlas::Lighting::Fog>();
        scene->fog->enable = true;
        scene->fog->density = 0.0002f;
        scene->fog->heightFalloff = 0.0284f;
        scene->fog->height = 0.0f;

        scene->sky.atmosphere = Atlas::CreateRef<Atlas::Lighting::Atmosphere>();

        scene->postProcessing.taa = Atlas::PostProcessing::TAA(0.99f);
        scene->postProcessing.sharpen.enable = true;
        scene->postProcessing.sharpen.factor = 0.15f;

        scene->sss = Atlas::CreateRef<Atlas::Lighting::SSS>();

        scene->ssgi = Atlas::CreateRef<Atlas::Lighting::SSGI>();

        scene->volumetric = Atlas::CreateRef<Atlas::Lighting::Volumetric>();

        scene->sky.clouds = Atlas::CreateRef<Atlas::Lighting::VolumetricClouds>();
        scene->sky.clouds->minHeight = 1400.0f;
        scene->sky.clouds->maxHeight = 1700.0f;
        scene->sky.clouds->castShadow = false;

        scene->physicsWorld = Atlas::CreateRef<Atlas::Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        scene->rayTracingWorld = Atlas::CreateRef<Atlas::RayTracing::RayTracingWorld>();

        return scene;

    }

}