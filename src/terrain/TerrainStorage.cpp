#include "TerrainStorage.h"

TerrainStorage::TerrainStorage(int32_t rootNodeCount, int32_t LODCount) {

	cells = new TerrainStorageCell*[LODCount];

	for (int32_t i = 0; i < LODCount; i++) {
		cells[i] = new TerrainStorageCell[rootNodeCount * powl(4, i)];
	}

}