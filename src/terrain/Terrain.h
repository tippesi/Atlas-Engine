#ifndef TERRAIN_H
#define TERRAIN_H

#include "../System.h"
#include "TerrainNode.h"
#include "TerrainStorage.h"

#include "../Camera.h"
#include "../Texture.h"
#include "../VertexArray.h"

#include <vector>

class Terrain {

public:
	Terrain(int32_t rootNodeCount, int32_t LoDCount, int32_t patchSizeFactor, float resolution, float height);

	void Update(Camera* camera);

	void SetLoDDistance(int32_t LoD, float distance);

	// vector<TerrainNode*> GetNodes(int32_t x, int32_t y, int32_t width, int32_t height);

	void Bind();

	void Unbind();

	TerrainStorage* storage;

	vec3 translation;
	float resolution;

	int32_t patchVertexCount;
	int32_t patchSize;
	vector<vec2> patchOffsets;

	float height;

	vector<TerrainNode*> renderList;

private:
	void GeneratePatchVertexBuffer(int32_t patchSizeFactor);

	void GeneratePatchOffsets(int32_t patchSizeFactor);

	vector<vec2> vertices;

	VertexArray* vertexArray;

	int32_t rootNodeCount;
	int32_t LoDCount;

	vector<float> LoDDistances;
	vector<TerrainNode*> rootNodes;

};


#endif