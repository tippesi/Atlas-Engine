#ifndef ACTOR_H
#define ACTOR_H

#include "../System.h"
#include "Mesh.h"

class Actor {

public:
	///
	/// \param mesh
	Actor(Mesh* mesh);

	mat4 modelMatrix;
	mat4 transformedMatrix;

	Mesh* mesh;

	bool render;
	bool castShadow;

};

#endif