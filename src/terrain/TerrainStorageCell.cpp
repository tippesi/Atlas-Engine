#include "TerrainStorageCell.h"
#include "TerrainStorage.h"

namespace Atlas {

	namespace Terrain {

		TerrainStorageCell::TerrainStorageCell(TerrainStorage* storage) : storage(storage) {

			

		}

		bool TerrainStorageCell::IsLoaded() {

			if (heightField == nullptr)
				return false;

			if (normalMap == nullptr)
				return false;

			return true;

		}

	}

}