#ifndef TERRAINNODE_H
#define TERRAINNODE_H

#include "../System.h"
#include "TerrainPatch.h"
#include "TerrainStorage.h"
#include "Camera.h"

#include <vector>

class TerrainNode {

public:
	TerrainNode(vec2 location, float resolution, float height, int32_t LoD, int32_t LoDCount,
		vec2 parentIndex, vec2 relativeIndex, TerrainStorage* storage, TerrainStorageCell* cell);

	void Update(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

	~TerrainNode();

private:
	void CreateChildren();

	void UpdateChildren(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

	void DeleteChildren();

	vec2 location;
	vec2 index;
	vec2 absoluteIndex;

	int32_t LoD;
	int32_t LoDCount;

	float resolution;
	float height;

	vector<TerrainNode*> children;

	TerrainStorage* storage;
	TerrainStorageCell* cell;

};

#endif