#include "VegetationGenerator.h"

#include "tools/TerrainTool.h"

#include <glm/gtx/polar_coordinates.hpp> 
#include <glm/gtx/quaternion.hpp>

namespace Atlas::Editor {

    void VegetationGenerator::GenerateAll(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator) {

        if (!scene->terrain.IsLoaded())
            return;

        Ref<Common::Image<uint16_t>> heightImage, moistureImage;
        terrainGenerator.GenerateHeightAndMoistureImages(heightImage, moistureImage);

        auto splatImage = Tools::TerrainTool::GenerateSplatMap(scene->terrain.Get());

        Volume::AABB aabb(scene->terrain->translation, scene->terrain->translation +
            vec3(scene->terrain->sideLength, scene->terrain->heightScale, scene->terrain->sideLength));
        octree = Volume::Octree<VegetationInstance>(aabb, 7);

        for (auto& type : types) {
            BeginGenerationOnType(scene, terrainGenerator, *heightImage, *moistureImage, type);
        }

        bool generatingOffspring = false;
        int32_t iteration = 0;
        do {
            generatingOffspring = false;
            iteration++;
            for (auto& type : types) {
                if (type.iterations < iteration)
                    continue;

                PerformIterationOnType(scene, terrainGenerator, *heightImage, *moistureImage,
                    *splatImage, type.randGenerator, type);

                generatingOffspring = true;
            }
        } while (generatingOffspring);

        for (auto& type : types) {
            EndGenerationOnType(scene, terrainGenerator, *heightImage, *moistureImage, type);
        }

        octree.Clear();

    }

    void VegetationGenerator::RemoveEntitiesFromScene(VegetationType& type, Ref<Scene::Scene>& scene) {

        for (auto ecsEntity : type.entities) {

            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid())
                continue;

            scene->DestroyEntity(entity);

        }

        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);
        if (parentEntity.IsValid())
            scene->DestroyEntity(parentEntity, false);

    }

    void VegetationGenerator::BeginGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type) {

        if (!scene->terrain.IsLoaded() || !type.mesh.IsLoaded())
            return;

        type.randGenerator = std::mt19937(type.seed * glm::clamp(seed, 0, 255));

        RemoveEntitiesFromScene(type, scene);

        type.entities.clear();
        type.instances.clear();

        auto rootEntity = scene->GetEntityByName("Root");

        Scene::Entity parentEntity = scene->CreateEntity();
        parentEntity.AddComponent<HierarchyComponent>();
        parentEntity.AddComponent<NameComponent>(type.name + " Generated");

        if (rootEntity.IsValid() && rootEntity.HasComponent<HierarchyComponent>()) {
            auto& hierarchy = rootEntity.GetComponent<HierarchyComponent>();
            hierarchy.AddChild(parentEntity);
        }

        type.parentEntity = parentEntity;

    }

    void VegetationGenerator::EndGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type) {

        if (!scene->terrain.IsLoaded() || !type.mesh.IsLoaded())
            return;

        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);
        auto& hierachy = parentEntity.GetComponent<HierarchyComponent>();

        const float alignmentLimit = glm::half_pi<float>() - type.maxAlignmentAngle;

        for (auto& instance : type.instances) {

            auto entity = scene->CreateEntity();

            vec3 scale = mix(type.scaleMin, type.scaleMax, GenerateUniformRandom(type.randGenerator));

            instance.scale *= glm::mix(type.growthMinScale, type.growthMaxScale,
                glm::clamp(float(instance.age) / float(type.growthMaxAge), 0.0f, 1.0f));

            mat4 rot{ 1.0f };
            if (type.alignToSurface) {
                vec3 N = glm::normalize(instance.normal);

                float cosTheta = glm::dot(N, vec3(0.0f, 1.0f, 0.0f));
                cosTheta = glm::clamp(cosTheta, -1.0f, 1.0f);

                float currentAngle = std::acos(cosTheta);
                if (currentAngle > type.maxAlignmentAngle) {
                    auto spherical = glm::polar(N);
                    spherical.x = alignmentLimit;

                    N = glm::euclidean(vec2(spherical.x, spherical.y));
                }

                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                glm::quat rotation = glm::rotation(up, glm::normalize(N));
                rot = glm::toMat4(rotation);
            }

            mat4 matrix(1.0f);
            matrix = translate(matrix, instance.position);
            matrix *= rot;
            matrix = glm::scale(matrix, instance.scale);

            auto& transform = entity.AddComponent<TransformComponent>(matrix);
            transform.globalMatrix = matrix;

            entity.AddComponent<MeshComponent>(type.mesh);

            type.entities.push_back(entity);

            hierachy.AddChild(Scene::Entity(entity, &scene->entityManager));

        }

    }

    void VegetationGenerator::PerformIterationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg,
         Common::Image<uint8_t>& splatImage, std::mt19937& randGenerator, VegetationType& type) {

        if (!scene->terrain.IsLoaded() || !type.mesh.IsLoaded())
            return;

        struct SpawnPoint {
            vec3 position;
            vec3 normal;
        };

        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);

        auto& terrain = scene->terrain;
        auto& hierarchy = parentEntity.GetComponent<HierarchyComponent>();

        std::vector<VegetationInstance> octreeInstances;

        float invSideLength = 1.0f / terrain->sideLength;
        auto heightScale = terrain->heightScale * float(heightImg.width) / float(terrainGenerator.previewSize);

        auto biomes = terrainGenerator.SortBiomes();

        auto spawnInstance = [&](vec2 offset, float scale, VegetationInstance& instance, Volume::AABB& aabb) -> bool {
            instance = {
                .position = vec3(
                    (2.0f * GenerateUniformRandom(randGenerator) - 1.0f) * scale + offset.x,
                    0.0f,
                    (2.0f * GenerateUniformRandom(randGenerator) - 1.0f) * scale + offset.y
                    )
            };

            float height, slope, moisture;
            terrainGenerator.GetBiomeIndicators(instance.position.x, instance.position.z, heightImg,
                moistureImg, heightScale, height, slope, moisture);

            if (height < type.heightMin || height > type.heightMax ||
                slope < type.slopeMin || slope > type.slopeMax)
                return false;

            auto materialIdx = splatImage.Sample(instance.position.x, instance.position.z).r;

            if (type.exludeMaterialIndices.contains(int32_t(materialIdx)))
                return false;

            instance.position = (instance.position * terrain->sideLength) + terrain->translation;

            vec3 forward;
            instance.position.y = terrain->GetHeight(instance.position.x, instance.position.z, instance.normal, forward);

            if (instance.position.y == terrain->invalidHeight)
                return false;

            octreeInstances.clear();

            aabb = type.mesh->data.aabb.Translate(instance.position);
            octree.QueryAABB(octreeInstances, aabb);

            for (auto& octreeInstance : octreeInstances) {
                auto distance = glm::distance(instance.position, octreeInstance.position);

                auto collisionDistance = distance;
                collisionDistance -= octreeInstance.type->collisionRadius;
                collisionDistance -= type.collisionRadius;

                if (collisionDistance < 0.0f)
                    return false;

                // No need to check for shade, continue
                if (type.canGrowInShade)
                    continue;

                auto shadeDistance = distance;
                shadeDistance -= octreeInstance.type->shadeRadius;
                shadeDistance -= type.shadeRadius;

                if (shadeDistance < 0.0f)
                    return false;

            }

            instance.age = 0;
            instance.type = &type;
            instance.scale = mix(type.scaleMin, type.scaleMax, GenerateUniformRandom(randGenerator));
            instance.position.y += type.offset.y;

            return true;
            };

        if (type.instances.empty()) {

            // The offset is only applied initially
            int32_t initialCount = int32_t(float(initialCountPerType) * type.initialDensity);
            auto typeInitialOffset = vec2(type.offset.x, type.offset.z) * invSideLength;
            for (int32_t i = 0; i < initialCount; i++) {

                Volume::AABB aabb;
                VegetationInstance instance;
                if (!spawnInstance(vec2(0.5f) + typeInitialOffset, 0.5f, instance, aabb))
                    continue;

                octree.Insert(instance, aabb);
                type.instances.push_back(instance);

            }

        }
        else {

            float spawnScale = type.offspringSpreadRadius * invSideLength;

            for (size_t i = 0; i < type.instances.size(); i++) {

                auto& instance = type.instances[i];
                instance.age++;

                bool hasOffspring = type.offspringPerIteration >= GenerateUniformRandom(randGenerator);
                if (!hasOffspring)
                    continue;

                //Scene::Entity entity(ecsEntity, &scene->entityManager);
                //auto& meshComponent = entity.GetComponent<MeshComponent>();

                auto position = instance.position;

                position -= terrain->translation;
                position *= invSideLength;

                Volume::AABB aabb;
                VegetationInstance offspring;
                if (!spawnInstance(vec2(position.x, position.z), spawnScale, offspring, aabb))
                    continue;

                octree.Insert(offspring, aabb);
                type.instances.push_back(offspring);

            }

        }

    }

    float VegetationGenerator::GenerateUniformRandom(std::mt19937& randGenerator) {

        return float(randGenerator()) / float(randGenerator.max());

    }

}
