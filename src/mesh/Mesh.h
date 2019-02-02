#ifndef AE_MESH_H
#define AE_MESH_H

#include "../System.h"
#include "MeshData.h"
#include "buffer/VertexArray.h"

#define AE_STATIONARY_MESH 0
#define AE_MOVABLE_MESH 1

class Mesh {

public:
	///
	/// \param data
	Mesh(MeshData* data, int32_t mobility = AE_STATIONARY_MESH);

	///
	/// \param filename
	Mesh(std::string filename, int32_t mobility = AE_STATIONARY_MESH);


	~Mesh();

	///
	void UpdateData();

	///
	void Bind();

	///
	void Unbind();

	///
	void DeleteContent();

	MeshData* const data;

	bool cullBackFaces = true;

	const int32_t mobility;

private:
	void InitializeVertexArray();

	VertexArray vertexArray;

};

#endif