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
            float rotation = 0.0f;
            int32_t age = 0;
            VegetationType* type;
        };

        struct ProposalVegetationInstance {
            VegetationInstance instance;
            Volume::AABB aabb;
        };

        struct VegetationType {
            size_t id = 0;

            std::string name;

            vec3 offset = vec3(0.0f);

            float slopeMin = 0.0f;
            float slopeMax = 1.0f;

            float heightMin = 0.0f;
            float heightMax = 1.0f;

            bool excludeBasedOnCorners = true;
            std::set<int32_t> exludeMaterialIndices;

            vec3 scaleMin = vec3(1.0f);
            vec3 scaleMax = vec3(1.0f);

            float rotationMin = 0.0f;
            float rotationMax = 2.0f * 3.14f;

            int32_t seed = int32_t(Common::Random::SampleUniformInt(0, (1 << 24)));
            
            int32_t iterations = 10;
            float initialDensity = 0.5f;
            float offspringPerIteration = 0.5f;
            float offspringSpreadRadius = 10.0f;

            float priority = 1.0f;

            bool alignToSurface = true;
            bool alignBasedOnCorners = false;
            float maxAlignmentAngle = 3.14f / 2.0f;

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
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, 
            Common::Image<uint8_t>& splatImage, VegetationType& type);

        void RemoveEntitiesFromScene(VegetationType& type, Ref<Scene::Scene>& scene);
        
        std::vector<VegetationType> types;

        const int32_t initialCountPerType = 10000;

        int32_t seed = 1;
        size_t typeCounter = 0;

    private:
        void BeginGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type);

        void EndGenerationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type);

        void PerformIterationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            std::vector<ProposalVegetationInstance>& proposalInstances,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, 
            Common::Image<uint8_t>& splatImage, std::mt19937& randGenerator, VegetationType& type);

        float GenerateUniformRandom(std::mt19937& randGenerator);

        Volume::Octree<VegetationInstance> octree;

    };

}