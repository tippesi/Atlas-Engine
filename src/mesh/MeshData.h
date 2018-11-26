#ifndef MESHDATA_H
#define MESHDATA_H

#include "../System.h"
#include "DataComponent.h"
#include "Material.h"

#include <vector>

#define PRIMITIVE_TRIANGLES GL_TRIANGLES
#define PRIMITIVE_TRIANGLE_STRIP GL_TRIANGLE_STRIP

#ifdef ENGINE_OGL
#define PRIMITIVE_QUADS GL_QUADS
#endif

typedef struct SubData {
	uint32_t indicesOffset;
	uint32_t numIndices;
	uint32_t materialIndex;
}SubData;

class MeshData {

public:
	///
	MeshData();

	///
	/// \param count
	void SetIndexCount(int32_t count);

	///
	/// \return
	int32_t GetIndexCount();

	///
	/// \param count
	void SetVertexCount(int32_t count);

	///
	/// \return
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