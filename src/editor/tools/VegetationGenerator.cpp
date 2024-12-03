#include "VegetationGenerator.h"

namespace Atlas::Editor {

    void VegetationGenerator::GenerateAll(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator) {

        Ref<Common::Image<uint16_t>> heightImage, moistureImage;
        terrainGenerator.GenerateHeightAndMoistureImages(heightImage, moistureImage);

        for (auto& type : types) {
            GenerateType(scene, terrainGenerator, *heightImage, *moistureImage, type);
        }

    }

    void VegetationGenerator::GenerateType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type) {

        if (!scene->terrain.IsLoaded() || !type.mesh.IsLoaded())
            return;

        std::mt19937 randGenerator(type.seed);

        RemoveEntitiesFromScene(type, scene);

        type.entities.clear();
        type.positions.clear();

        Scene::Entity parentEntity = scene->CreateEntity();
        parentEntity.AddComponent<HierarchyComponent>();
        parentEntity.AddComponent<NameComponent>(type.name + " Generated");

        for (int32_t i = 0; i < iterations; i++) {
            PerformIterationOnType(scene, terrainGenerator, heightImg, moistureImg,
                randGenerator, type);
        }

    }

    void VegetationGenerator::RemoveEntitiesFromScene(VegetationType& type, Ref<Scene::Scene>& scene) {

        for (auto ecsEntity : type.entities) {

            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid())
                continue;

            scene->DestroyEntity(entity);

        }

        Scene::Entity parentEntity(type.parentEntity, &scene->entityManager);
        scene->DestroyEntity(parentEntity, false);

    }

    void VegetationGenerator::PerformIterationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
        Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg,
        std::mt19937& randGenerator, VegetationType& type) {

        struct SpawnPoint {
            vec3 position;
            vec3 normal;
        };

        auto& terrain = scene->terrain;

        auto spawnPoint = [&](vec2 offset, float scale, SpawnPoint& point) -> bool {
            point = {
                .position = vec3(
                    (2.0f * GenerateUniformRandom(randGenerator) - 1.0f) * scale + offset.x,
                    0.0f,
                    (2.0f * GenerateUniformRandom(randGenerator) - 1.0f) * scale + offset.y
                    )
            };

            auto biome = terrainGenerator.GetBiome(point.position.x, point.position.z,
                terrainGenerator.elevationBiomes, heightImg, moistureImg, terrain->heightScale);

            if (biome.id != type.biomeId)
                return false;

            point.position = (point.position * terrain->sideLength) + terrain->translation;

            vec3 forward;
            point.position.y = terrain->GetHeight(point.position.x, point.position.z, point.normal, forward);

            if (point.position.y == terrain->invalidHeight)
                return false;

            return true;
            };

        std::vector<SpawnPoint> spawnPoints;
        if (type.entities.empty()) {

            int32_t initialCount = int32_t(float(initialCountPerType) * type.initialDensity);
            for (int32_t i = 0; i < initialCount; i++) {

                SpawnPoint point;
                if (!spawnPoint(vec2(0.5f), 0.5f, point))
                    continue;

                spawnPoints.push_back(point);

            }

        }
        else {

            float invSideLength = 1.0f / terrain->sideLength;
            float spawnScale = type.offspringSpreadRadius * invSideLength;

            for (size_t i = 0; i < type.entities.size(); i++) {

                bool hasOffspring = type.offspringPerIteration >= GenerateUniformRandom(randGenerator);
                if (!hasOffspring)
                    continue;

                //Scene::Entity entity(ecsEntity, &scene->entityManager);
                //auto& meshComponent = entity.GetComponent<MeshComponent>();

                auto position = type.positions[i];

                position -= terrain->translation;
                position *= invSideLength;

                SpawnPoint point;
                if (!spawnPoint(position, spawnScale, point))
                    continue;

                spawnPoints.push_back(point);

            }

        }

        for (const auto& point : spawnPoints) {

            auto entity = scene->CreateEntity();

            vec3 scale = glm::mix(type.scaleMin, type.scaleMax, GenerateUniformRandom(randGenerator));

            mat4 rot { 1.0f };
            if (type.alignToSurface) {
                vec3 N = point.normal;
                vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
                vec3 tangent = normalize(cross(up, N));
                vec3 bitangent = cross(N, tangent);

                rot = mat4(mat3(tangent, N, bitangent));
            }

            glm::mat4 matrix;
            matrix = glm::scale(scale);
            matrix = glm::translate(matrix, point.position);

            entity.AddComponent<TransformComponent>(matrix);
            entity.AddComponent<MeshComponent>(type.mesh);

            type.entities.push_back(entity);
            type.positions.push_back(point.position);

        }

    }

    float VegetationGenerator::GenerateUniformRandom(std::mt19937& randGenerator) {

        return float(randGenerator()) / float(randGenerator.max());

    }

}