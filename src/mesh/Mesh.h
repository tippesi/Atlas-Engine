#ifndef MESH_H
#define MESH_H

#include "../System.h"
#include "MeshData.h"

class Mesh {

public:
	Mesh(MeshData* data);

	Mesh(string filename);

	void UpdateData();

	void Bind();

	void Unbind();

	~Mesh();

	MeshData* data;

private:
	void InitializeVBO();
	void InitializeVAO();

	uint32_t vao;
	uint32_t indicesVbo;
	uint32_t verticesVbo;
	uint32_t texCoordsVbo;
	uint32_t normalsVbo;
	uint32_t tangentsVbo;

};

#endif