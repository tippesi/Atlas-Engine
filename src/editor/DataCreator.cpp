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

    // A sphere with smooth normals and enough vertices to show the displacement of a graph
    Ref<Mesh::Mesh> DataCreator::CreateSphere(const ResourceHandle<Material>& material, int32_t rings, int32_t segments) {

        const float radius = 1.0f;

        Mesh::MeshData data;
        data.indices.SetType(Mesh::ComponentFormat::UnsignedInt);
        data.vertices.SetType(Mesh::ComponentFormat::Float);
        data.normals.SetType(Mesh::ComponentFormat::PackedNormal);
        data.texCoords.SetType(Mesh::ComponentFormat::HalfFloat);
        data.tangents.SetType(Mesh::ComponentFormat::PackedNormal);

        auto vertexCount = (rings + 1) * (segments + 1);
        auto indexCount = rings * segments * 6;
        data.SetVertexCount(vertexCount);
        data.SetIndexCount(indexCount);
        data.vertices.SetElementCount(vertexCount);
        data.normals.SetElementCount(vertexCount);
        data.texCoords.SetElementCount(vertexCount);
        data.tangents.SetElementCount(vertexCount);
        data.indices.SetElementCount(indexCount);

        for (int32_t ring = 0; ring <= rings; ring++) {
            for (int32_t segment = 0; segment <= segments; segment++) {
                auto idx = ring * (segments + 1) + segment;
                auto v = float(ring) / float(rings);
                auto u = float(segment) / float(segments);
                auto theta = v * glm::pi<float>();
                auto phi = u * 2.0f * glm::pi<float>();
                auto normal = vec3(glm::sin(theta) * glm::cos(phi), glm::cos(theta), glm::sin(theta) * glm::sin(phi));

                data.vertices[idx] = normal * radius;
                data.normals[idx] = vec4(normal, 0.0f);
                data.texCoords[idx] = vec2(u * 2.0f, v);
                data.tangents[idx] = vec4(-glm::sin(phi), 0.0f, glm::cos(phi), 1.0f);
            }
        }

        int32_t index = 0;
        for (int32_t ring = 0; ring < rings; ring++) {
            for (int32_t segment = 0; segment < segments; segment++) {
                auto a = uint32_t(ring * (segments + 1) + segment);
                auto b = uint32_t((ring + 1) * (segments + 1) + segment);
                for (auto vertex : { a, a + 1, b, a + 1, b + 1, b })
                    data.indices[index++] = vertex;
            }
        }

        data.aabb = Volume::AABB(vec3(-radius), vec3(radius));
        data.materials = { material };
        data.subData = { Mesh::MeshSubData {
            .name = "Sphere",
            .indicesOffset = 0,
            .indicesCount = uint32_t(indexCount),
            .material = material,
            .materialIdx = 0,
            .aabb = data.aabb,
            .uvScale = radius * glm::pi<float>()
            } };

        auto mesh = CreateRef<Mesh::Mesh>(data);
        // Only for looking at the material, the ray tracing data would take a moment to build
        mesh->rayTrace = false;
        mesh->name = "Detailed sphere";

        return mesh;

    }

}
