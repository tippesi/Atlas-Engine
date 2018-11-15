#ifndef TERRAINSTORAGE_H
#define TERRAINSTORAGE_H

#include "../System.h"
#include "../Texture.h"

typedef struct TerrainStorageCell {

	Texture* heightField;

}TerrainStorageCell;

class TerrainStorage {

public:
	TerrainStorage(int32_t rootNodeCount, int32_t LoDCount);

	TerrainStorageCell* GetCell(int32_t x, int32_t y, int32_t LoD);

	void AddHeightfield(Texture* heightField, int32_t xSector, int32_t ySector, int32_t LoD);

private:
	int32_t LoDCount;
	int32_t rootNodeCount;

	TerrainStorageCell** cells;

};


#endif