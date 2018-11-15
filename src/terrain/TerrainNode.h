#ifndef TERRAINNODE_H
#define TERRAINNODE_H

#include "../System.h"
#include "TerrainPatch.h"
#include "TerrainStorage.h"
#include "Camera.h"

#include <vector>

class TerrainNode {

public:
	TerrainNode(vec2 location, int32_t terrainResolution, int32_t LoD, vec2 index, TerrainStorage* storage);

	void Update(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

private:
	vec2 location;
	vec2 index;

	int32_t terrainResolution;
	int32_t LoD;

	TerrainNode* children;

	TerrainStorage* storage;
	TerrainStorageCell* cell;

};

#endif