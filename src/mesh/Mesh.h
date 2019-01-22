#ifndef MESH_H
#define MESH_H

#include "../System.h"
#include "MeshData.h"
#include "buffer/VertexArray.h"

#define STATIONARY_MESH 0
#define MOVABLE_MESH 1

class Mesh {

public:
	///
	/// \param data
	Mesh(MeshData* data, int32_t mobility = STATIONARY_MESH);

	///
	/// \param filename
	Mesh(string filename, int32_t mobility = STATIONARY_MESH);


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