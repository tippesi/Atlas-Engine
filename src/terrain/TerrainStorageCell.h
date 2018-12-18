#ifndef TERRAINSTORAGECELL_H
#define TERRAINSTORAGECELL_H

#include "../System.h"
#include "../texture/Texture2D.h"

#include <vector>

/**
 * Stores the material information for a terrain node.
 * Only LoD0 uses a splatmap and the materials
 */
class TerrainStorageCell {

public:
	TerrainStorageCell();

	bool IsLoaded();

	int32_t x;
	int32_t y;
	int32_t LoD;

	vector<uint8_t> heightData;

	Texture2D* heightField;
	Texture2D* normalMap;
	Texture2D* diffuseMap;
	Texture2D* displacementMap;

};

#endif