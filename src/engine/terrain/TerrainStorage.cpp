#include <algorithm>
#include "TerrainStorage.h"

namespace Atlas {

	namespace Terrain {

		TerrainStorage::TerrainStorage(int32_t rootNodeCount, int32_t LoDCount, float sideLength,
			int32_t materialResolution, int32_t materialCount) : rootNodeCount(rootNodeCount), 
			LoDCount(LoDCount), materialResolution(materialResolution), materialCount(materialCount) {

			cells.resize(LoDCount);
			LoDSideLengths = new int32_t[LoDCount];

            /*
			baseColorMaps = Atlas::Texture::Texture2DArray(materialResolution,
				materialResolution, materialCount, AE_RGB8, GL_REPEAT, GL_LINEAR, true, true);
			roughnessMaps = Atlas::Texture::Texture2DArray(materialResolution,
				materialResolution, materialCount, AE_R8, GL_REPEAT, GL_LINEAR, true, true);
			aoMaps = Atlas::Texture::Texture2DArray(materialResolution,
				materialResolution, materialCount, AE_R8, GL_REPEAT, GL_LINEAR, true, true);
			normalMaps = Atlas::Texture::Texture2DArray(materialResolution,
				materialResolution, materialCount, AE_RGB8, GL_REPEAT, GL_LINEAR, true, true);
			displacementMaps = Atlas::Texture::Texture2DArray(materialResolution,
				materialResolution, materialCount, AE_R8, GL_REPEAT, GL_LINEAR, true, true);
            */

			materials.resize(materialCount);

			for (int32_t i = 0; i < LoDCount; i++) {

				cells[i] = std::vector<TerrainStorageCell>(rootNodeCount * (int32_t)powf(4, (float)i), this);
				LoDSideLengths[i] = (int32_t)sqrtf((float)rootNodeCount * powf(4.0f, (float)i));

				for (int32_t x = 0; x < LoDSideLengths[i]; x++) {
					for (int32_t y = 0; y < LoDSideLengths[i]; y++) {
						cells[i][x * LoDSideLengths[i] + y].x = x;
						cells[i][x * LoDSideLengths[i] + y].y = y;
						cells[i][x * LoDSideLengths[i] + y].LoD = i;
						cells[i][x * LoDSideLengths[i] + y].position = vec2((float)x, (float)y) * sideLength;
					}
				}

				sideLength /= 2.0f;

			}

		}

		TerrainStorageCell* TerrainStorage::GetCell(int32_t x, int32_t y, int32_t LoD) {

			if (x < 0 || x >= LoDSideLengths[LoD] || y < 0 || y >= LoDSideLengths[LoD])
				return nullptr;

			return &cells[LoD][x * LoDSideLengths[LoD] + y];

		}

		int32_t TerrainStorage::GetCellCount(int32_t LoD) {

			return (int32_t)cells[LoD].size();

		}

		void TerrainStorage::AddMaterial(int32_t slot, Material* material) {

			materials[slot] = material;

            /*
			if (material->HasBaseColorMap()) {
				baseColorMaps.Copy(*material->baseColorMap, 0, 0, 0, 0, 0, slot,
					baseColorMaps.width, baseColorMaps.height, 1);
			}
			if (material->HasRoughnessMap()) {
				roughnessMaps.Copy(*material->roughnessMap, 0, 0, 0, 0, 0, slot,
					roughnessMaps.width, roughnessMaps.height, 1);
			}
			if (material->HasAoMap()) {
				aoMaps.Copy(*material->aoMap, 0, 0, 0, 0, 0, slot,
					aoMaps.width, aoMaps.height, 1);
			}
			if (material->HasNormalMap()) {
				normalMaps.Copy(*material->normalMap, 0, 0, 0, 0, 0, slot,
					normalMaps.width, normalMaps.height, 1);
			}
			if (material->HasDisplacementMap()) {
				displacementMaps.Copy(*material->displacementMap, 0, 0, 0, 0, 0, slot,
					displacementMaps.width, displacementMaps.height, 1);
			}

			baseColorMaps.Bind();
			baseColorMaps.GenerateMipmap();

			roughnessMaps.Bind();
			roughnessMaps.GenerateMipmap();

			aoMaps.Bind();
			aoMaps.GenerateMipmap();

			normalMaps.Bind();
			normalMaps.GenerateMipmap();

			displacementMaps.Bind();
			displacementMaps.GenerateMipmap();
             */

		}

		void TerrainStorage::RemoveMaterial(int32_t slot, Material* material) {

			materials[slot] = nullptr;

		}

		std::vector<Material*> TerrainStorage::GetMaterials() {

			return materials;

		}

	}

}