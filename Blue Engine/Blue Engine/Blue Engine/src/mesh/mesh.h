#ifndef MESH_H
#define MESH_H

#include "../system.h"
#include "meshdata.h"

class Mesh {

public:
	Mesh(MeshData* data);

	Mesh(const char* filename);

	MeshData* data;

};

#endif