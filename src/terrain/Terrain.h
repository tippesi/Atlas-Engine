#ifndef TERRAIN_H
#define TERRAIN_H

#include "../System.h"
#include "TerrainNode.h"
#include "TerrainStorage.h"

#include "../Camera.h"
#include "../Texture.h"

#include <vector>

class Terrain {

public:
	Terrain(int32_t rootNodeCount, int32_t LODCount, int32_t patchSize);

	void AddHeightfield(Texture* heightField, int32_t x, int32_t y);

	void Update(Camera* camera);

	TerrainStorage* storage;

private:
	void GeneratePatchVertexBuffer();

	int32_t rootNodeCount;
	int32_t LODCount;
	int32_t patchSize;

	vector<TerrainNode*> rootNodes;

};


#endif