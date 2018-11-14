#ifndef TERRAINSTORGAE_H
#define TERRAINSTORAGE_H

#include "../System.h"
#include "../Texture.h"

typedef struct TerrainStorageCell {

	Texture* heightField;

}TerrainStorageCell;

class TerrainStorage {

public:
	TerrainStorage(int32_t rootNodeCount, int32_t LODCount);

	void AddHeightfield(Texture* heightField, int32_t xSector, int32_t ySector, int32_t LOD);

private:
	TerrainStorageCell** cells;

};


#endif