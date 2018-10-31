#ifndef ACTORBATCH_H
#define ACTORBATCH_H

#include "../system.h"
#include "actor.h"
#include "mesh.h"

#include <vector>

class ActorBatch {

public:
	ActorBatch(Mesh* mesh);

	void Add(Actor* actor);

	void Remove(Actor* actor);

	Mesh* GetMesh();

private:
	vector<Actor*> actors;

	Mesh* mesh;

};

#endif