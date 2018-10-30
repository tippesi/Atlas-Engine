#include "mesh.h"

Mesh::Mesh(MeshData* data) : data(data) {

	glGenBuffers(1, &indices);
	glGenBuffers(1, &vertices);
	glGenBuffers(1, &texCoords);
	glGenBuffers(1, &normals);
	glGenBuffers(1, &tangents);



}

Mesh::Mesh(const char* filename) {



}