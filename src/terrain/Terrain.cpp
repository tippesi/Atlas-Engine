#include "Terrain.h"

Terrain::Terrain(int32_t rootNodeCount, int32_t LODCount, int32_t patchSize, int32_t terrainResolution) : 
	rootNodeCount(rootNodeCount), patchSize(patchSize), terrainResolution(terrainResolution) {

	// sum{k = 0 to LODCount - 1} 4^k = (4^(LODCount) - 1) / 3
	int32_t nodesCount = (int32_t)(powl(4, LODCount) - 1) / 3 * rootNodeCount;

	// We can just have 2^16 nodes due to 16 bit indexing
	if (nodesCount > powl(2, 16)) {
		// We have to adjust the LOD Count
		this->LODCount = floorl(logl(3 * powl(2, 16) / rootNodeCount + 1) / logl(4));
	}
	else {
		this->LODCount = LODCount;
	}

	storage = new TerrainStorage(this->rootNodeCount, this->LODCount);

}