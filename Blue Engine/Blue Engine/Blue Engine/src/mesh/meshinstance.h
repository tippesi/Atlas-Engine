#ifndef MESHINSTANCE_H
#define MESHINSTANCE_H

#include "../system.h"
#include "mesh.h"

class MeshInstance {

public:
	MeshInstance(Mesh* mesh);

	mat4 modelMatrix;

	Mesh* mesh;

	bool render;
	bool castShadow;

};

#endif