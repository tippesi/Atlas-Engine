#include <algorithm>
#include "TerrainStorage.h"

namespace Atlas {

	namespace Terrain {

		TerrainStorage::TerrainStorage(int32_t rootNodeCount, int32_t LoDCount) : rootNodeCount(rootNodeCount), LoDCount(LoDCount) {

			cells.resize(LoDCount);
			LoDSideLengths = new int32_t[LoDCount];

			for (int32_t i = 0; i < LoDCount; i++) {

				cells[i] = std::vector<TerrainStorageCell>(rootNodeCount * (int32_t)powf(4, (float)i), this);
				LoDSideLengths[i] = (int32_t)sqrtf((float)rootNodeCount * powf(4.0f, (float)i));

				for (int32_t x = 0; x < LoDSideLengths[i]; x++) {
					for (int32_t y = 0; y < LoDSideLengths[i]; y++) {
						cells[i][x * LoDSideLengths[i] + y].x = x;
						cells[i][x * LoDSideLengths[i] + y].y = y;
						cells[i][x * LoDSideLengths[i] + y].LoD = i;
					}
				}
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

		int32_t TerrainStorage::GetMaterialIndex(Material* material) {

			size_t count = 0;
			while (count < materials.size()) {
				if (material == materials[count].first)
					break;
				count++;
			}

			if (count != materials.size()) {
				materials[count].second++;
				return (int32_t)count;
			}

			materials.push_back(std::pair<Material*, int32_t>(material, 1));
			return (int32_t)materials.size() - 1;

		}

		void TerrainStorage::RemoveMaterial(int32_t index) {

			if (index >= 0 && index < (int32_t)materials.size())
				if (!--materials[index].second)
					materials.erase(materials.begin() + index);				

		}

		Material* TerrainStorage::GetMaterial(int32_t index) {

			if (index >= 0 && index < (int32_t)materials.size())
				return materials[index].first;

			return nullptr;

		}

		std::vector<Material*> TerrainStorage::GetMaterials() {

			std::vector<Material*> materials;

			for (auto& pair : this->materials) {
				materials.push_back(pair.first);
			}

			return materials;

		}

	}

}