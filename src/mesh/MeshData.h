#ifndef MESHDATA_H
#define MESHDATA_H

#include "../System.h"
#include "DataComponent.h"
#include "Material.h"

#include <vector>

#define AE_PRIMITIVE_TRIANGLES GL_TRIANGLES
#define AE_PRIMITIVE_TRIANGLE_STRIP GL_TRIANGLE_STRIP

typedef struct MeshSubData {
	uint32_t indicesOffset;
	uint32_t numIndices;
	uint32_t materialIndex;
}MeshSubData;

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

	DataComponent<uint32_t, void>* indices;
	
	DataComponent<float, float>* vertices;
	DataComponent<float, float16>* texCoords;
	DataComponent<float, uint32_t>* normals;
	DataComponent<float, uint32_t>* tangents;

	vector<Material*> materials;
	vector<MeshSubData*> subData;

	int32_t primitiveType;

private:
	int32_t indexCount;
	int32_t vertexCount;

};

#endif