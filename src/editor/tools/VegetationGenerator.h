#pragma once

#include "scene/Scene.h"
#include "common/RandomHelper.h"
#include "TerrainGenerator.h"

#include <vector>

namespace Atlas::Editor {

    class VegetationGenerator {

    public:
        struct VegetationType {
            size_t id = 0;

            std::string name;

            vec3 scaleMin = vec3(1.0f);
            vec3 scaleMax = vec3(1.0f);

            int32_t seed = Common::Random::SampleUniformInt(0, (1 << 24));

            int32_t biomeId = 0;
            
            float initialDensity = 0.5f;
            float offspringPerIteration = 0.5f;
            float offspringSpreadRadius = 10.0f;

            bool alignToSurface = true;

            ResourceHandle<Mesh::Mesh> mesh;

            ECS::Entity parentEntity;
            std::vector<ECS::Entity> entities;
            std::vector<vec3> positions;
        };

        VegetationGenerator() = default;

        void GenerateAll(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator);

        void GenerateType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, VegetationType& type);

        void RemoveEntitiesFromScene(VegetationType& type, Ref<Scene::Scene>& scene);
        
        int32_t iterations = 4;
        std::vector<VegetationType> types;

        const int32_t initialCountPerType = 10000;
        size_t typeCounter = 0;

    private:
        void PerformIterationOnType(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, 
            std::mt19937& randGenerator, VegetationType& type);

        float GenerateUniformRandom(std::mt19937& randGenerator);

    };

}