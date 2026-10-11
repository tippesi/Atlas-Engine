#pragma once

#include "../System.h"
#include "../terrain/Terrain.h"
#include "../common/Image.h"
#include "../Filter.h"

namespace Atlas {

    namespace Tools {

        class TerrainTool {

        public:
            /**
             * Generates a terrain from an image with height data
             * @param heightImage
             * @param rootNodeSideCount
             * @param LoDCount
             * @param patchSize
             * @param resolution
             * @param height
             * @param material A standard material which is applied.
             * @return A pointer to a Terrain object.
             * @warning The input should correspond to the terrain specifications
             */
            static Ref<Terrain::Terrain> GenerateTerrain(Common::Image<uint16_t>& heightImage, int32_t rootNodeSideCount, int32_t LoDCount,
                int32_t patchSize, float resolution, float height, ResourceHandle<Material> material);


            static Ref<Terrain::Terrain> GenerateTerrain(Common::Image<uint16_t>& heightImage, Common::Image<uint8_t>& splatImage,
                int32_t rootNodeSideCount, int32_t LoDCount, int32_t patchSize, float resolution,
                float height, std::vector<ResourceHandle<Material>> materials);

             /**
             *
             * @param terrain
             * @warning All storage cells of the terrain and their heightData member must be loaded.
             * It is assumed that all cells have textures of the same resolution.
             */
            static void UpdateTerrain(const Ref<Terrain::Terrain>& terrain, Common::Image<uint8_t>& splatImage, 
                std::vector<ResourceHandle<Material>> materials);

            /**
             *
             * @param terrain
             * @warning All storage cells of the terrain and their heightData member must be loaded.
             * It is assumed that all cells have textures of the same resolution.
             */
            static void BakeTerrain(const Ref<Terrain::Terrain>& terrain);

            /**
             *
             * @param terrain
             * @param filter
             * @param strength
             * @param position
             * @warning All max LoD storage cells of the terrain and their heightData member
             * must be loaded. It is assumed that all cells have textures of the same resolution.
             * @note The kernel size needs to be smaller than 2 times the edge of a cell
             */
            static void BrushHeight(const Ref<Terrain::Terrain>& terrain, Filter* filter, float strength, vec2 position);

            /**
             *
             * @param terrain
             * @param size
             * @param contributingRadius
             * @param strength
             * @param position
             */
            static void SmoothHeight(const Ref<Terrain::Terrain>& terrain, int32_t size, int32_t contributingRadius,
                float strength, vec2 position);

            static void FlattenHeight(const Ref<Terrain::Terrain>& terrain, int32_t size, float strength, 
                vec2 position, float height, bool circularBrush = true);

            static void BrushHole(const Ref<Terrain::Terrain>& terrain, vec2 position, int32_t size,
                bool circularBrush = true);

            static void BrushMaterial(const Ref<Terrain::Terrain>& terrain, vec2 position, int32_t size, int32_t slot,
                bool circularBrush = true);

            static Texture::Texture2D GenerateTerrainOceanMap(const Ref<Terrain::Terrain>& terrain, float oceanHeight, int32_t resolution);

            static Ref<Common::Image<uint16_t>> GenerateHeightMap(const Ref<Terrain::Terrain>& terrain);

            static Ref<Common::Image<uint8_t>> GenerateSplatMap(const Ref<Terrain::Terrain>& terrain);

            static void LoadMissingCells(const Ref<Terrain::Terrain>& terrain, const std::string& filename);

        private:
            static void GenerateNormalData(const std::vector<uint16_t>& heightData, std::vector<uint8_t>& normalData,
                                           int32_t width, int32_t height, float strength);

            static float GetHeight(const std::vector<uint16_t>& heightData, int32_t dataWidth,
                    int32_t x, int32_t y, int32_t width, int32_t height, float scale);

            static bool GetNearbyStorageCells(const Ref<Terrain::Terrain>& terrain, vec2 position,
                Terrain::TerrainStorageCell** cells);

            static void ExtractNearbyStorageData(const Ref<Terrain::Terrain>& terrain, Terrain::TerrainStorageCell** cells,
                std::span<float> heightData, std::span<uint8_t> splatData);

            static void ApplyDataToNearbyStorage(const Ref<Terrain::Terrain>& terrain, Terrain::TerrainStorageCell** cells,
                std::span<float> heightData, std::span<uint8_t> splatData);

            static Common::Image<uint8_t> CalculateHoleMap(int32_t resolution, const Common::Image<uint16_t>& heightMap);

        };

    }

}