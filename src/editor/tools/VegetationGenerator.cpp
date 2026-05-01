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

        std::vector<ProposalVegetationInstance> proposalInstances(initialCountPerType);

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

                PerformIterationOnType(scene, terrainGenerator, proposalInstances,
                    *heightImage, *moistureImage, *splatImage, type.randGenerator, type);

                generatingOffspring = true;
            }
        } while (generatingOffspring);

        for (auto& type : types) {
            EndGenerationOnType(scene, terrainGenerator, *heightImage, *moistureImage, type);
        }

        octree.Clear();

    }

    void VegetationGenerator::RemoveEntitiesFromScene(VegetationType& type, Ref<Scene::Scene>& scene) {
       
        // Destroy parent recursive
        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);
        if (parentEntity.IsValid())
            scene->DestroyEntity(parentEntity, true);

    }

    void VegetationGenerator::BeginGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type) {

        if (!scene->terrain.IsLoaded() || !scene->entityManager.Valid(type.entity))
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

        if (!scene->terrain.IsLoaded() || !scene->entityManager.Valid(type.entity))
            return;

        Scene::Entity sourceEntity(type.entity, &scene->entityManager);
        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);
        auto& hierachy = parentEntity.GetComponent<HierarchyComponent>();

        const float alignmentLimit = glm::half_pi<float>() - type.maxAlignmentAngle;

        for (auto& instance : type.instances) {

            auto entity = scene->DuplicateEntity(sourceEntity);

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
                rot = glm::toMat4(rotation) * glm::rotate(instance.rotation, up);
            }

            mat4 matrix(1.0f);
            matrix = translate(matrix, instance.position);
            matrix *= rot;
            matrix = glm::scale(matrix, instance.scale);

            if (!entity.HasComponent<TransformComponent>())
                entity.AddComponent<TransformComponent>();

            auto& transform = entity.GetComponent<TransformComponent>();
            transform.Set(matrix);
            transform.globalMatrix = matrix;

            type.entities.push_back(entity);

            hierachy.AddChild(Scene::Entity(entity, &scene->entityManager));

        }

    }

    void VegetationGenerator::PerformIterationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        std::vector<ProposalVegetationInstance>& proposalInstances, Common::Image<uint16_t>& heightImg, 
        Common::Image<uint16_t>& moistureImg, Common::Image<uint8_t>& splatImage, std::mt19937& randGenerator,
        VegetationType& type) {

        if (!scene->terrain.IsLoaded() || !scene->entityManager.Valid(type.entity))
            return;

        struct SpawnPoint {
            vec3 position;
            vec3 normal;
        };

        Scene::Entity entity(type.entity, &scene->entityManager);
        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);

        auto& terrain = scene->terrain;
        auto& hierarchy = parentEntity.GetComponent<HierarchyComponent>();

        std::vector<VegetationInstance> octreeInstances;

        float invSideLength = 1.0f / terrain->sideLength;
        auto heightScale = terrain->heightScale * float(heightImg.width) / float(terrainGenerator.previewSize);

        auto biomes = terrainGenerator.SortBiomes();
        auto mesh = entity.HasComponent<MeshComponent>() ?
               entity.GetComponent<MeshComponent>().mesh : ResourceHandle<Mesh::Mesh>();

        // Proposal spawner
        auto spawnProposalInstance = [&](vec2 offset, float scale, ProposalVegetationInstance& propInstance) -> bool {
            auto& instance = propInstance.instance;
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

            instance.position = (instance.position * terrain->sideLength) + terrain->translation;
            instance.rotation = glm::mix(type.rotationMin, type.rotationMax, GenerateUniformRandom(randGenerator));
            instance.scale = glm::mix(type.scaleMin, type.scaleMax, GenerateUniformRandom(randGenerator));

            if (type.excludeBasedOnCorners && mesh.IsLoaded()) {
                glm::mat4 transform {1.0f};
                transform = translate(transform, instance.position);
                transform *= glm::rotate(instance.rotation, vec3(0.0f, 1.0f, 0.0f));
                transform = glm::scale(transform, instance.scale);

                propInstance.aabb = mesh->data.aabb.Transform(transform);

                auto min = propInstance.aabb.min;
                auto max = propInstance.aabb.max;
                vec2 corners[] = { vec2(min.x, min.z), vec2(min.x, max.z),
                    vec2(max.x, min.z), vec2(max.x, max.z) };
                for (int32_t i = 0; i < 4; i++) {
                    auto position = (corners[i] - vec2(terrain->translation.x, terrain->translation.z)) 
                        * invSideLength;
                    auto materialIdx = splatImage.Sample(position.x, position.y).r;
                    if (type.exludeMaterialIndices.contains(int32_t(materialIdx)))
                        return false;
                }
            }
            else {
                auto materialIdx = splatImage.Sample(instance.position.x, instance.position.z).r;

                if (type.exludeMaterialIndices.contains(int32_t(materialIdx)))
                    return false;
            }

            vec3 forward;
            instance.position.y = terrain->GetHeight(instance.position.x, instance.position.z, instance.normal, forward);

            if (instance.position.y == terrain->invalidHeight)
                return false;

            instance.age = 0;
            instance.type = &type;
            instance.position.y += type.offset.y;

            return true;
            };

        // Check for other instances that might have spawned
        auto canSpawnInstance = [&](ProposalVegetationInstance& propInstance) -> bool {
            auto& instance = propInstance.instance;
            octreeInstances.clear();

            octree.QueryAABB(octreeInstances, propInstance.aabb);

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

            return true;
            };


        std::int32_t instanceCounter = 0;

        if (type.instances.empty()) {

            // The offset is only applied initially
            int32_t initialCount = int32_t(float(initialCountPerType) * type.initialDensity);
            auto typeInitialOffset = vec2(type.offset.x, type.offset.z) * invSideLength;
            for (int32_t i = 0; i < initialCount; i++) {
                ProposalVegetationInstance instance;
                if (!spawnProposalInstance(vec2(0.5f) + typeInitialOffset, 0.5f, instance))
                    continue;

                proposalInstances[instanceCounter++] = instance;
            }
        }
        else {

            float spawnScale = type.offspringSpreadRadius * invSideLength;

            // Check for enough space
            if (proposalInstances.size() < type.instances.size()) {
                proposalInstances.resize(type.instances.size());
            }

            for (int32_t i = 0; i < int32_t(type.instances.size()); i++) {
                auto& instance = type.instances[i];
                instance.age++;

                bool hasOffspring = type.offspringPerIteration >= GenerateUniformRandom(randGenerator);
                if (!hasOffspring)
                    continue;

                auto position = instance.position;

                position -= terrain->translation;
                position *= invSideLength;

                ProposalVegetationInstance offspring;
                if (!spawnProposalInstance(vec2(position.x, position.z), spawnScale, offspring))
                    continue;

                proposalInstances[instanceCounter++] = offspring;
            }
        }

        auto count = instanceCounter;
        for (int32_t i = 0; i < count; i++) {
            auto& proposalInstance = proposalInstances[i];

            if (!canSpawnInstance(proposalInstance)) 
                continue;

            octree.Insert(proposalInstance.instance, proposalInstance.aabb);
            type.instances.push_back(proposalInstance.instance);
        }

    }

    float VegetationGenerator::GenerateUniformRandom(std::mt19937& randGenerator) {

        return float(randGenerator()) / float(randGenerator.max());

    }

}
