#pragma once

#include <terrain/Terrain.h>
#include <Material.h>
#include <texture/Texture2D.h>

#include "../ui/panels/ResourceSelectionPanel.h"
#include "ImguiExtension/panels/MaterialPanel.h"

namespace Atlas::Editor {

    class TerrainGenerator {

    public:
        struct SlopeBiome {
            float slope;

            int32_t materialIdx = 0;
        };

        struct MoistureBiome {
            float moisture;

            int32_t materialIdx = 0;
        };

        struct ElevationBiome {
            float elevation;
            int32_t less;
            std::vector<MoistureBiome> moistureBiomes;
            std::vector<SlopeBiome> slopeBiomes;

            int32_t materialIdx = 0;
        };

        TerrainGenerator();

        ~TerrainGenerator();

        void Update();

        void Render(ResourceHandle<Terrain::Terrain>& terrain);

        ResourceHandle<Terrain::Terrain> GetTerrain();

        void UpdateHeightmapFromTerrain(ResourceHandle<Terrain::Terrain>& terrain);

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

    private:
        void Generate();
        void GeneratePreviews();
        std::vector<ElevationBiome> SortBiomes();
        std::pair<ResourceHandle<Material>, vec3> Biome(float x, float y, std::vector<ElevationBiome>& biomes,
            Common::Image<uint16_t>& heightImg, Common::Image<uint16_t>& moistureImg, float scale);

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