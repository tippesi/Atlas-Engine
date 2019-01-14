#ifndef TERRAINNODE_H
#define TERRAINNODE_H

#include "../System.h"
#include "TerrainStorage.h"
#include "Camera.h"

#include <vector>

class TerrainNode {

public:
	TerrainNode(vec2 location, float resolution, float height, float sideLength, int32_t LoD, int32_t LoDCount,
		ivec2 parentIndex, ivec2 relativeIndex, TerrainStorage* storage, TerrainStorageCell* cell);

	~TerrainNode();

	void Update(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

	vec2 location;
	float sideLength;
	ivec2 absoluteIndex;

	TerrainStorageCell* cell;

private:
	void CreateChildren();

	void UpdateChildren(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

	void DeleteChildren();

	ivec2 index;

	int32_t LoD;
	int32_t LoDCount;

	float resolution;
	float height;

	vector<TerrainNode> children;

	TerrainStorage* storage;

};

#endif