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

	vector<uint8_t> heightData;

	Texture* heightField;
	Texture* normalMap;
	Texture* diffuseMap;
	Texture* displacementMap;

};

#endif