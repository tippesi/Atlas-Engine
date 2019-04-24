#include "TerrainStorageCell.h"
#include "TerrainStorage.h"

namespace Atlas {

	namespace Terrain {

		TerrainStorageCell::TerrainStorageCell(TerrainStorage* storage) : storage(storage) {

			// -1 means there is no material
			for (int32_t i = 0; i < 4; i++)
				materialIndices[i] = -1;

		}

		bool TerrainStorageCell::IsLoaded() {

			if (heightField == nullptr)
				return false;

			if (normalMap == nullptr)
				return false;

			return true;

		}

		void TerrainStorageCell::SetMaterial(Material* material, int32_t slot) {

			if (slot >= 0 && slot <= 3) {
				if (materialIndices[slot] >= 0)
					storage->RemoveMaterial(materialIndices[slot]);
				materialIndices[slot] = storage->GetMaterialIndex(material);
			}

		}

		void TerrainStorageCell::RemoveMaterial(int32_t slot) {

			if (slot >= 0 && slot <= 3) {
				if (materialIndices[slot] >= 0)
					storage->RemoveMaterial(materialIndices[slot]);
				materialIndices[slot] = -1;
			}

		}

		Material* TerrainStorageCell::GetMaterial(int32_t slot) {

			if (slot >= 0 && slot <= 3) {
				if (materialIndices[slot] >= 0)
					return storage->GetMaterial(materialIndices[slot]);
			}

			return nullptr;

		}

		std::vector<Material*> TerrainStorageCell::GetMaterials() {

			std::vector<Material*> materials;

			for (int32_t i = 0; i < 4; i++) {
				if (materialIndices[i] >= 0)
					materials.push_back(storage->GetMaterial(materialIndices[i]));
			}

			return materials;

		}

	}

}