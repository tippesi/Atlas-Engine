#ifndef MESHDATA_H
#define MESHDATA_H

#include "../system.h"
#include "datacomponent.h"

typedef struct SubData {
	uint32_t offset;
	uint32_t numIndices;
	uint32_t materialIndex;
}SubData;

class MeshData {

public:
	MeshData();

	void SetIndexCount(int32_t count);

	int32_t GetIndexCount();

	void SetVertexCount(int32_t count);

	int32_t GetVertexCount();

	~MeshData();

	DataComponent<uint32_t>* indices;
	
	DataComponent<float>* vertices;
	DataComponent<float>* texCoords;
	DataComponent<float>* normals;
	DataComponent<float>* tangents;

private:
	int32_t indexCount;
	int32_t vertexCount;

};

#endif