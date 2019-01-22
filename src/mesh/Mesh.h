#ifndef MESH_H
#define MESH_H

#include "../System.h"
#include "MeshData.h"
#include "buffer/VertexArray.h"

class Mesh {

public:
	///
	/// \param data
	Mesh(MeshData* data);

	///
	/// \param filename
	Mesh(string filename);


	~Mesh();

	///
	void UpdateData();

	///
	void Bind();

	///
	void Unbind();

	///
	void DeleteContent();

	MeshData* data;

	bool cullBackFaces = true;

private:
	void InitializeVertexArray();

	VertexArray vertexArray;

};

#endif