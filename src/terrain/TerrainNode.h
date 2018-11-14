#ifndef TERRAINNODE_H
#define TERRAINNODE_H

#include "../System.h"
#include "TerrainPatch.h"
#include "TerrainStorage.h"
#include "Camera.h"

#include <vector>

class TerrainNode {

public:
	TerrainNode(vec2 location, int32_t terrainResolution, int32_t LoD, vec2 index);

	void Update(Camera* camera, vector<TerrainNode*>* renderList);

private:
	int32_t terrainResolution;

	TerrainStorage* storage;
	TerrainStorageCell* cell;

};

#endif