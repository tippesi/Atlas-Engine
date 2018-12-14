#ifndef MESH_H
#define MESH_H

#include "../System.h"
#include "MeshData.h"
#include "VertexArray.h"

class Mesh {

public:
	///
	/// \param data
	Mesh(MeshData* data);

	///
	/// \param filename
	Mesh(string filename);

	///
	void UpdateData();

	///
	void Bind();

	///
	void Unbind();

	///
	void DeleteContent();

	~Mesh();

	MeshData* data;

	bool cullBackFaces;

private:
	void InitializeVertexArray();

	VertexArray* vertexArray;

};

#endif