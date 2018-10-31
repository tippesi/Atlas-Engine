#ifndef MESHDATA_H
#define MESHDATA_H

#include "../system.h"
#include "datacomponent.h"
#include "material.h"

#include <vector>

#define PRIMITIVE_TRIANGLES 0x0004
#define PRIMITIVE_TRIANGLE_STRIP 0x0005

#ifdef ENGINE_OGL
#define PRIMITIVE_QUADS 0x0007
#endif

typedef struct SubData {
	uint32_t indicesOffset;
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

	vector<Material*> materials;
	vector<SubData*> subData;

	int32_t primitiveType;

private:
	int32_t indexCount;
	int32_t vertexCount;

};

#endif