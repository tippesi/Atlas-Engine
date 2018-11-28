#ifndef TERRAINSTORAGECELL_H
#define TERRAINSTORAGECELL_H

#include "../System.h"
#include "../Texture.h"

class TerrainStorageCell {

public:
	TerrainStorageCell();

	bool IsLoaded();

	int32_t x;
	int32_t y;
	int32_t LoD;

	uint8_t* heightData;

	Texture* heightField;
	Texture* normalMap;

};

#endif