#ifndef TERRAINNODE_H
#define TERRAINNODE_H

#include "../System.h"
#include "TerrainStorage.h"
#include "Camera.h"

#include <vector>

class TerrainNode {

public:
	TerrainNode(vec2 location, float resolution, float height, float sideLength, int32_t LoD, int32_t LoDCount,
		vec2 parentIndex, vec2 relativeIndex, TerrainStorage* storage, TerrainStorageCell* cell);

	void Update(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

	~TerrainNode();

	vec2 location;
	float sideLength;
	vec2 absoluteIndex;

	TerrainStorageCell* cell;

private:
	void CreateChildren();

	void UpdateChildren(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances);

	void DeleteChildren();

	vec2 index;


	int32_t LoD;
	int32_t LoDCount;

	float resolution;
	float height;

	vector<TerrainNode*> children;

	TerrainStorage* storage;

};

#endif