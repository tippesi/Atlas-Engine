#ifndef AE_TERRAINTOOL_H
#define AE_TERRAINTOOL_H

#include "../System.h"
#include "../terrain/Terrain.h"
#include "../loader/ImageLoader.h"
#include "../Kernel.h"

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
             * @return A pointer to a Terrain object.
             * @warning The input should correspond to the terrain specifications
             */
			static Terrain::Terrain* GenerateTerrain(Loader::Image16& heightImage, int32_t rootNodeSideCount, int32_t LoDCount,
					int32_t patchSize, float resolution, float height);

			/**
             *
             * @param terrain
             * @warning All storage cells of the terrain and their heightData member must be loaded.
             * It is assumed that all cells have textures of the same resolution.
             */
			static void BakeTerrain(Terrain::Terrain* terrain);

			/**
             *
             * @param terrain
             * @param kernel
             * @param strength
             * @param position
             * @warning All max LoD storage cells of the terrain and their heightData member
             * must be loaded. It is assumed that all cells have textures of the same resolution.
             * @note The kernel size needs to be smaller than 2 times the edge of a cell
             */
			static void BrushHeight(Terrain::Terrain* terrain, Kernel* kernel, float strength, vec2 position);

			/**
			 *
			 * @param terrain
			 * @param size
			 * @param contributingRadius
			 * @param strength
			 * @param position
			 */
			static void SmoothHeight(Terrain::Terrain* terrain, int32_t size, int32_t contributingRadius,
					float strength, vec2 position);

			static void BrushMaterial(Terrain::Terrain* terrain, Kernel* kernel, float strength, vec2 position);

		private:
			static void GenerateNormalData(std::vector<uint16_t>& heightData, std::vector<uint8_t>& normalData,
										   int32_t width, int32_t height, float strength);

			static float GetHeight(std::vector<uint16_t>& heightData, int32_t dataWidth,
					int32_t x, int32_t y, int32_t width, int32_t height);

		};

	}

}

#endif