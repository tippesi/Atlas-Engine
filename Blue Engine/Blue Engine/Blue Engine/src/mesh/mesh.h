#ifndef MESH_H
#define MESH_H

#include "../system.h"
#include "meshdata.h"

class Mesh {

public:
	Mesh(MeshData* data);

	Mesh(const char* filename);

	uint32_t GetVAO();

	MeshData* data;

private:
	uint32_t vao;
	uint32_t indices;
	uint32_t vertices;
	uint32_t texCoords;
	uint32_t normals;
	uint32_t tangents;

};

#endif