#include "Terrain.h"

Terrain::Terrain(int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize, float resolution, float height) : 
	resolution(resolution), height(height) {

	translation = vec3(0.0f);
	this->patchSize = patchSize;

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

	GeneratePatchVertexBuffer(this->patchSize);

	storage = new TerrainStorage(this->rootNodeCount, this->LoDCount);
	LoDDistances = new float[LoDCount];

	float terrainSideLength = (float)nodesPerSide * resolution * powf(2, (float)this->LoDCount - 1.0f) * this->patchSize * 8.0f;
	float ratio = terrainSideLength / (float)nodesPerSide;

	for (int32_t i = 0; i < this->LoDCount; i++) {
		LoDDistances[i] = terrainSideLength - powf((float)i / (float)this->LoDCount, 0.25f) * terrainSideLength;
	}

	for (int32_t i = 0; i < nodesPerSide; i++) {
		for (int32_t j = 0; j < nodesPerSide; j++) {
			TerrainStorageCell* cell = storage->GetCell(i, j, 0);
			storage->requestedCells.push_back(cell);
			rootNodes.push_back(new TerrainNode(vec2((float)i * ratio, (float)j * ratio), resolution, height, ratio, 
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

void Terrain::Bind() {

	vertexArray->Bind();

}

void Terrain::Unbind() {
	
	vertexArray->Unbind();

}

void Terrain::GeneratePatchVertexBuffer(int32_t patchSizeFactor) {

	patchVertexCount = (int32_t)powf((float)(4 * patchSizeFactor), 2.0f);

	vertices = new vec2[patchVertexCount];

	int32_t index = 0;

	for (int32_t x = 0; x < patchSizeFactor; x++) {
		for (int32_t y = 0; y < patchSizeFactor; y++) {
			vertices[index++] = vec2(0.0f, 0.0f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.333333f, 0.0f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.666666f, 0.0f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(1.0f, 0.0f) + vec2((float)x, (float)y);

			vertices[index++] = vec2(0.0f, 0.333333f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.333333f, 0.333333f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.666666f, 0.333333f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(1.0f, 0.333333f) + vec2((float)x, (float)y);

			vertices[index++] = vec2(0.0f, 0.666666f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.333333f, 0.666666f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.666666f, 0.666666f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(1.0f, 0.666666f) + vec2((float)x, (float)y);

			vertices[index++] = vec2(0.0f, 1.0f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.333333f, 1.0f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(0.666666f, 1.0f) + vec2((float)x, (float)y);
			vertices[index++] = vec2(1.0f, 1.0f) + vec2((float)x, (float)y);
		}
	}

	vertexArray = new VertexArray();
	VertexBuffer* vertexBuffer = new VertexBuffer(GL_ARRAY_BUFFER, GL_FLOAT, 2);
	vertexBuffer->SetData(vertices, patchVertexCount);
	vertexArray->AddComponent(0, vertexBuffer);
	glPatchParameteri(GL_PATCH_VERTICES, 16);

}