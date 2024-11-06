#pragma once

#include <terrain/Terrain.h>
#include <Material.h>
#include <texture/Texture2D.h>

#include "../ui/panels/ResourceSelectionPanel.h"

namespace Atlas::Editor {

    class TerrainGenerator {

    public:
        TerrainGenerator();

        void Update();

        void Render();

        void Show();

        bool IsVisible();

        void Clear();

        Ref<Terrain::Terrain> GetTerrain();

    private:
        struct SlopeBiome {
            float slope;

            int32_t selection;
            ResourceHandle<Material> material;
        };

        struct MoistureBiome {
            float moisture;

            int32_t selection;
            ResourceHandle<Material> material;
        };

        struct ElevationBiome {
            float elevation;
            int32_t less;
            std::vector<MoistureBiome> moistureBiomes;
            std::vector<SlopeBiome> slopeBiomes;

            int32_t selection;
            ResourceHandle<Material> material;
        };

        std::vector<ElevationBiome> SortBiomes();
        std::pair<ResourceHandle<Material>, vec3> Biome(float x, float y, std::vector<ElevationBiome>& biomes,
            Ref<Common::Image<uint16_t>>& heightImg, Ref<Common::Image<uint16_t>>& moistureImg, float scale);

        UI::ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;
        UI::ResourceSelectionPanel<Material> materialSelectionPanel;

        ResourceHandle<Texture::Texture2D> heightMap;
        Ref<Common::Image<uint16_t>> heightMapImage;

        JobGroup previewMapGenerationJob;

        ResourceHandle<Material> selectedMaterial;
        std::vector<std::pair<ResourceHandle<Material>, vec3>> materials;
        std::vector<ElevationBiome> elevationBiomes;

        Texture::Texture2D previewHeightMap;
        Texture::Texture2D previewMoistureMap;
        Texture::Texture2D previewBiomeMap;

        Texture::Texture2D newPreviewHeightMap;
        Texture::Texture2D newPreviewMoistureMap;
        Texture::Texture2D newPreviewBiomeMap;

        Ref<Common::Image<uint16_t>> previewHeightImg;
        Ref<Common::Image<uint16_t>> previewMoistureImg;
        Ref<Common::Image<uint8_t>> previewBiomeImg;

        int32_t heightMapResolution = 2048;
        int32_t resolutionSelection = 2;

        std::vector<float> heightAmplitudes;
        float heightExp = 1.0f;
        int32_t heightSeed = 0;

        std::vector<float> moistureAmplitudes;
        int32_t moistureSeed = 1;

        std::string name;
        int32_t LoDCount = 6;
        float resolution = 1.0f;
        float height = 300.0f;

        int32_t materialSelection = 0;
        int32_t loadFromFile = 0;

        bool advanced = false;
        bool visible = false;
        bool successful = false;

    };

}