#include "TerrainStorage.h"

TerrainStorage::TerrainStorage(int32_t rootNodeCount, int32_t LoDCount) : rootNodeCount(rootNodeCount), LoDCount(LoDCount) {

	cells = new TerrainStorageCell*[LoDCount];

	for (int32_t i = 0; i < LoDCount; i++) {
		cells[i] = new TerrainStorageCell[rootNodeCount * (int32_t)powf(4, i)];
	}

}

TerrainStorageCell* TerrainStorage::GetCell(int32_t x, int32_t y, int32_t LoD) {

	return &cells[LoD][x * (int32_t)sqrtf((float)rootNodeCount * powf(4.0f, (float)LoD)) + y];

}