#pragma once

#include <terrain/Terrain.h>
#include <Material.h>
#include <texture/Texture2D.h>
#include <common/RandomHelper.h>

#include "../ui/panels/ResourceSelectionPanel.h"
#include "ImguiExtension/panels/MaterialPanel.h"

namespace Atlas::Editor {

    class TerrainGenerator {

    public:
        struct Biome {
            size_t id = size_t(Common::Random::SampleUniformInt(0, (1 << 31)));
            int32_t materialIdx = -1;
        };

        struct SlopeBiome : Biome {
            float slope;
        };

        struct MoistureBiome : Biome {
            float moisture;
        };

        struct ElevationBiome : Biome  {
            float elevation;
            int32_t less;
            std::vector<MoistureBiome> moistureBiomes;
            std::vector<SlopeBiome> slopeBiomes;
        };

        TerrainGenerator();

        ~TerrainGenerator();

        void Update();

        void Render(ResourceHandle<Terrain::Terrain>& terrain);

        ResourceHandle<Terrain::Terrain> GetTerrain();

        void UpdateHeightmapFromTerrain(ResourceHandle<Terrain::Terrain>& terrain);

        void UpdateHeightmapFromFile();

        void GenerateHeightAndMoistureImages(Ref<Common::Image<uint16_t>>& heightImg,
        Ref<Common::Image<uint16_t>>& moistureImg);

        Biome GetBiome(float x, float y, std::vector<ElevationBiome>& biomes,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, float scale);

        std::vector<ElevationBiome> SortBiomes();

        ResourceHandle<Texture::Texture2D> heightMap;
        ResourceHandle<Material> selectedMaterial;
        std::vector<std::pair<ResourceHandle<Material>, vec3>> materials;
        std::vector<ElevationBiome> elevationBiomes;

        std::vector<float> heightAmplitudes;
        float heightExp = 1.0f;
        int32_t heightSeed = 0;

        std::vector<float> moistureAmplitudes;
        int32_t moistureSeed = 1;

        std::string name;
        int32_t LoDCount = 6;
        int32_t patchSize = 8;
        float resolution = 1.0f;
        float height = 300.0f;

        int32_t resolutionSelection = 2;
        int32_t materialSelection = 0;
        int32_t heightMapSelection = 0;

        bool advanced = false;

        const int32_t previewSize = 128;

    private:
        void Generate();
        void GeneratePreviews();        
        bool IsBiomeValid(const Biome& biome) const;

        ImguiExtension::MaterialPanel materialPanel;

        UI::ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;
        UI::ResourceSelectionPanel<Material> materialSelectionPanel;
        
        Ref<Common::Image<uint16_t>> newHeightMapImage;
        Ref<Common::Image<uint16_t>> heightMapImage;

        JobGroup previewMapGenerationJob;
        JobGroup heightMapUpdateJob;

        Texture::Texture2D previewHeightMap;
        Texture::Texture2D previewMoistureMap;
        Texture::Texture2D previewBiomeMap;

        Texture::Texture2D newPreviewHeightMap;
        Texture::Texture2D newPreviewMoistureMap;
        Texture::Texture2D newPreviewBiomeMap;

        
        Ref<Common::Image<uint16_t>> previewHeightImg;
        Ref<Common::Image<uint16_t>> previewMoistureImg;
        Ref<Common::Image<uint8_t>> previewBiomeImg;

        bool successful = false;

    };

}