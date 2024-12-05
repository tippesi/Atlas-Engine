#pragma once

#include "scene/Scene.h"
#include "common/RandomHelper.h"
#include "TerrainGenerator.h"
#include "volume/Octree.h"

#include <vector>

namespace Atlas::Editor {

    class VegetationGenerator {

    public:
        struct VegetationType;

        struct VegetationInstance {
            vec3 position = vec3(0.0f);
            vec3 normal = vec3(0.0f, 1.0f, 0.0f);
            vec3 scale = vec3(1.0f);
            int32_t age = 0;
            VegetationType* type;
        };

        struct VegetationType {
            size_t id = 0;

            std::string name;

            vec3 offset = vec3(0.0f);

            vec3 scaleMin = vec3(1.0f);
            vec3 scaleMax = vec3(1.0f);

            int32_t seed = int32_t(Common::Random::SampleUniformInt(0, (1 << 24)));

            int32_t biomeId = 0;
            
            int32_t iterations = 4;
            float initialDensity = 0.5f;
            float offspringPerIteration = 0.5f;
            float offspringSpreadRadius = 10.0f;

            float priority = 1.0f;

            bool alignToSurface = true;

            float collisionRadius = 1.0f;
            float shadeRadius = 1.0f;

            bool canGrowInShade = false;
            int32_t growthMaxAge = 10;

            float growthMinScale = 1.0f;
            float growthMaxScale = 1.5f;

            ResourceHandle<Mesh::Mesh> mesh;

            ECS::Entity parentEntity = ECS::EntityConfig::InvalidEntity;
            std::vector<ECS::Entity> entities;
            std::vector<VegetationInstance> instances;

            std::mt19937 randGenerator;
        };        

        VegetationGenerator() = default;

        void GenerateAll(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator);

        void GenerateType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type);

        void RemoveEntitiesFromScene(VegetationType& type, Ref<Scene::Scene>& scene);
        
        std::vector<VegetationType> types;

        const int32_t initialCountPerType = 10000;
        size_t typeCounter = 0;

    private:
        void BeginGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type);

        void EndGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type);

        void PerformIterationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, 
            std::mt19937& randGenerator, VegetationType& type);

        float GenerateUniformRandom(std::mt19937& randGenerator);

        Volume::Octree<VegetationInstance> octree;

    };

}