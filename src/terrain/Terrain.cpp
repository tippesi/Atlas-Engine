#include "Terrain.h"

Terrain::Terrain(int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize, float resolution, float height) : 
	patchSize(patchSize), resolution(resolution), height(height) {

	translation = vec3(0.0f);

	// Just in case the input was somehow wrong
	int32_t nodesPerSide = (int32_t)floor(sqrtf((float)rootNodeCount));
	this->rootNodeCount = (int32_t)powf((float)nodesPerSide, 2.0f);

	// sum{k = 0 to LODCount - 1} 4^k = (4^(LODCount) - 1) / 3
	int32_t nodesCount = (int32_t)((powf(4.0f, (float)LoDCount) - 1.0f) / 3.0f) * this->rootNodeCount;

	// We can just have 2^16 nodes due to 16 bit indexing
	if (nodesCount >= 65536) {
		// We have to adjust the LOD Count
		this->LoDCount = (int32_t)(logf(3.0f * powf(2.0f, 16.0f) / (float)this->rootNodeCount + 1.0f) / logf(4.0f));
	}
	else {
		this->LoDCount = LoDCount;
	}

	storage = new TerrainStorage(this->rootNodeCount, this->LoDCount);
	LoDDistances = new float[LoDCount];

	float terrainSideLength = (resolution * powf(2, (float)this->LoDCount)) * patchSize * 8.0f;
	float ratio = terrainSideLength / (float)nodesPerSide;

	for (int32_t i = 0; i < this->LoDCount; i++) {
		LoDDistances[i] = (float)i / this->LoDCount * terrainSideLength;
	}

	for (int32_t i = 0; i < nodesPerSide; i++) {
		for (int32_t j = 0; j < nodesPerSide; j++) {
			TerrainStorageCell* cell = storage->GetCell(i, j, 0);
			rootNodes.push_back(new TerrainNode(vec2((float)i * ratio, (float)j * ratio), resolution, height, 
				0, this->LoDCount, vec2(0, 0), vec2(i, j), storage, cell));
		}
	}

}

void Terrain::Update(Camera* camera) {

	renderList.clear();

	for (TerrainNode*& node : rootNodes) {
		node->Update(camera, renderList, LoDDistances);
	}

	// TODO: Sort renderlist by LoD here. Better: Have a list for each LoD

}

void Terrain::SetLoDDistance(int32_t LoD, float distance) {

	if (LoD >= 0 && LoD < LoDCount) {
		LoDDistances[LoD] = distance;
	}

}