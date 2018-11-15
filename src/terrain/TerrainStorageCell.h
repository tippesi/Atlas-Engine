#ifndef TERRAINSTORAGECELL_H
#define TERRAINSTORAGECELL_H

#include "../System.h"
#include "../Texture.h"

class TerrainStorageCell {

public:
	TerrainStorageCell();

	bool IsLoaded();

	void RemoveContent();

	~TerrainStorageCell();

	int32_t x;
	int32_t y;
	int32_t LoD;

	Texture* heightField;

};

#endif