#ifndef TERRAINSTORAGE_H
#define TERRAINSTORAGE_H

#include "../System.h"
#include "TerrainStorageCell.h"

#include <vector>

class TerrainStorage {

public:
	TerrainStorage(int32_t rootNodeCount, int32_t LoDCount);

	TerrainStorageCell* GetCell(int32_t x, int32_t y, int32_t LoD);

	vector<TerrainStorageCell*> requestedCells;
	vector<TerrainStorageCell*> unusedCells;

private:
    int32_t rootNodeCount;
	int32_t LoDCount;

	int32_t* LoDSideLengths;

	TerrainStorageCell** cells;

};


#endif