#ifndef ACTORBATCH_H
#define ACTORBATCH_H

#include "../System.h"
#include "Actor.h"
#include "Mesh.h"

#include <vector>

class ActorBatch {

public:
	ActorBatch(Mesh* mesh);

	void Add(Actor* actor);

	void Remove(Actor* actor);

	Mesh* GetMesh();

	vector<Actor*> actors;

private:
	Mesh* mesh;

};

#endif