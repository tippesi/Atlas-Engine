#include "TerrainStorageCell.h"
#include "TerrainStorage.h"

namespace Atlas {

	namespace Terrain {

		TerrainStorageCell::TerrainStorageCell(TerrainStorage* storage) : storage(storage) {

			std::memset(materialIndices, 0, sizeof(materialIndices));

		}

		bool TerrainStorageCell::IsLoaded() {

			if (heightField == nullptr)
				return false;

			if (normalMap == nullptr)
				return false;

			if (diffuseMap == nullptr)
				return false;

			if (displacementMap == nullptr)
				return false;

			return true;

		}

		void TerrainStorageCell::SetMaterial(Material* material, int32_t slot) {

			if (slot >= 0 && slot <= 3) {
				materialIndices[slot] = storage->GetMaterialIndex(material);
			}

		}

	}

}