#include "DataCreator.h"

#include "loader/ModelImporter.h"
#include "volume/AABB.h"

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

        directionalLight.properties.directional.direction = vec3(0.0f, -1.0f, 1.0f);
        directionalLight.color = vec3(255, 236, 209) / 255.0f;
        directionalLight.intensity = 10.0f;
        directionalLight.AddDirectionalShadow(200.0f, 3.0f, 2048, 0.05f, 3, 0.9f);
        directionalLight.isMain = true;

        mainHierarchy.AddChild(directionalLightEntity);

        scene->reflection = CreateRef<Lighting::Reflection>();

        scene->fog = CreateRef<Lighting::Fog>();
        scene->fog->enable = true;
        scene->fog->density = 0.0002f;
        scene->fog->heightFalloff = 0.0284f;
        scene->fog->height = 0.0f;

        scene->sky.atmosphere = CreateRef<Lighting::Atmosphere>();

        scene->postProcessing.taa = PostProcessing::TAA(0.99f);
        scene->postProcessing.sharpen.enable = true;
        scene->postProcessing.sharpen.factor = 0.15f;

        scene->irradianceVolume = CreateRef<Lighting::IrradianceVolume>(Volume::AABB(min, max), ivec3(15), 4);
        scene->irradianceVolume->rayCount = 32;
        scene->irradianceVolume->SetAABB(Volume::AABB(vec3(0.0f), vec3(200.0f)));
        scene->irradianceVolume->scroll = true;

        scene->sss = CreateRef<Lighting::SSS>();

        scene->ssgi = CreateRef<Lighting::SSGI>();
        scene->rtgi = CreateRef<Lighting::RTGI>();

        scene->sky.clouds = CreateRef<Lighting::VolumetricClouds>(512, 128, 32);
        scene->sky.clouds->baseAltitude = 1400.0f;
        scene->sky.clouds->layerThickness = 1400.0f;
        scene->sky.clouds->castShadow = false;

        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        return scene;

    }

    Ref<Scene::Scene> DataCreator::CreateSceneFromMesh(const std::string& filename, vec3 min, vec3 max,
        int32_t depth, bool invertUVs, bool addRigidBodies, bool makeMeshesStatic, Loader::ImportTextureFormat textureFormat) {

        auto scene = Loader::ModelImporter::ImportScene(filename, min, max, depth,
            true, makeMeshesStatic, invertUVs, 4096, textureFormat, true);

        auto rootEntity = scene->GetRootEntity();
        auto& mainHierarchy = rootEntity.GetComponent<HierarchyComponent>();

        auto directionalLightEntity = scene->CreateEntity();
        directionalLightEntity.AddComponent<NameComponent>("Directional light");
        auto& directionalLight = directionalLightEntity.AddComponent<LightComponent>(LightType::DirectionalLight);

        directionalLight.properties.directional.direction = vec3(0.0f, -1.0f, 1.0f);
        directionalLight.color = vec3(255, 236, 209) / 255.0f;
        directionalLight.intensity = 10.0f;
        directionalLight.AddDirectionalShadow(200.0f, 3.0f, 2048, 0.05f, 3, 0.9f);
        directionalLight.isMain = true;

        mainHierarchy.AddChild(directionalLightEntity);
        
        scene->reflection = CreateRef<Lighting::Reflection>();

        scene->fog = CreateRef<Lighting::Fog>();
        scene->fog->enable = true;
        scene->fog->density = 0.0002f;
        scene->fog->heightFalloff = 0.0284f;
        scene->fog->height = 0.0f;

        scene->sky.atmosphere = CreateRef<Lighting::Atmosphere>();

        scene->postProcessing.taa = PostProcessing::TAA(0.99f);
        scene->postProcessing.sharpen.enable = true;
        scene->postProcessing.sharpen.factor = 0.15f;

        scene->irradianceVolume = CreateRef<Lighting::IrradianceVolume>(Volume::AABB(min, max), ivec3(15), 4);
        scene->irradianceVolume->rayCount = 32;
        scene->irradianceVolume->SetAABB(Volume::AABB(vec3(0.0f), vec3(200.0f)));
        scene->irradianceVolume->scroll = true;

        scene->sss = CreateRef<Lighting::SSS>();

        scene->ssgi = CreateRef<Lighting::SSGI>();
        scene->rtgi = CreateRef<Lighting::RTGI>();

        scene->sky.clouds = CreateRef<Lighting::VolumetricClouds>(512, 128, 32);
        scene->sky.clouds->baseAltitude = 1400.0f;
        scene->sky.clouds->layerThickness = 1400.0f;
        scene->sky.clouds->castShadow = false;

        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();
        scene->postProcessing.upscaler = PostProcessing::Upscaler::FSR2;

        // This runs in a job while frames are running, only the main thread may update a scene. The global
        // transforms don't exist yet, the bodies are created once their meshes are imported
        std::function<void(Scene::Entity, const mat4&)> addBodies;
        addBodies = [&](Scene::Entity entity, const mat4& parentMatrix) {
            auto transformComponent = entity.TryGetComponent<TransformComponent>();
            auto globalMatrix = transformComponent ? parentMatrix * transformComponent->matrix : parentMatrix;

            auto meshComponent = entity.TryGetComponent<MeshComponent>();
            if (meshComponent && transformComponent) {
                Atlas::Physics::MeshShapeSettings settings = {
                    .mesh = meshComponent->mesh,
                    .scale = Common::MatrixDecomposition(globalMatrix).scale
                };
                auto shape = Atlas::Physics::ShapesManager::CreateShape(settings);

                auto bodySettings = Atlas::Physics::BodyCreationSettings{
                    .objectLayer = Atlas::Physics::Layers::Static,
                    .shape = shape,
                };
                entity.AddComponent<RigidBodyComponent>(bodySettings);
            }

            auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
            if (hierarchyComponent) {
                for (auto child : hierarchyComponent->GetChildren())
                    addBodies(child, globalMatrix);
            }
            };

        if (addRigidBodies)
            addBodies(rootEntity, mat4(1.0f));

        scene->physicsWorld->OptimizeBroadphase();

        return scene;

    }

}