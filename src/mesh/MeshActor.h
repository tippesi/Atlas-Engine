#ifndef ACTOR_H
#define ACTOR_H

#include "../System.h"
#include "Mesh.h"

class MeshActor {

public:
	/**
	 *
	 * @param mesh
	 */
	MeshActor(Mesh* mesh);

	mat4 modelMatrix;
	mat4 transformedMatrix;

	Mesh* mesh;

	bool castShadow;

	bool visible;

};

#endif