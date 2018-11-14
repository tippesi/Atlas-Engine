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
	Terrain(int32_t rootNodeCount, int32_t LODCount, int32_t patchSize, int32_t terrainResolution);

	void AddHeightfield(Texture* heightField, int32_t x, int32_t y);

	void Update(Camera* camera);

	vector<TerrainNode*> GetNodes(int32_t x, int32_t y, int32_t width, int32_t height);

	TerrainStorage* storage;

private:
	void GeneratePatchVertexBuffer();

	int32_t rootNodeCount;
	int32_t LODCount;
	int32_t patchSize;
	int32_t terrainResolution;

	vector<TerrainNode*> rootNodes;
	vector<TerrainNode*> renderList;

};


#endif