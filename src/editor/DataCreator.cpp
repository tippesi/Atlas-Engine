#include "DataCreator.h"

namespace Atlas::Editor {

    using namespace Scene::Components;
    using namespace Scene::Prefabs;

    Ref<Scene::Scene> DataCreator::CreateScene(const std::string &name, vec3 min, vec3 max, int32_t depth) {

        auto scene = CreateRef<Scene::Scene>(name, min, max, depth);

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

        scene->volumetric = CreateRef<Lighting::Volumetric>();

        scene->sky.clouds = CreateRef<Lighting::VolumetricClouds>();
        scene->sky.clouds->minHeight = 1400.0f;
        scene->sky.clouds->maxHeight = 1700.0f;
        scene->sky.clouds->castShadow = false;

        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        return scene;

    }

}