#ifndef ACTORBATCH_H
#define ACTORBATCH_H

#include "../System.h"
#include "Actor.h"
#include "Mesh.h"

#include <vector>

class ActorBatch {

public:
	///
	/// \param mesh
	ActorBatch(Mesh* mesh);

	///
	/// \param actor
	void Add(Actor* actor);

	///
	/// \param actor
	void Remove(Actor* actor);

	///
	/// \return
	Mesh* GetMesh();

	int32_t GetSize();

	///
	void ClearContent();

	///
	void DeleteContent();

	vector<Actor*> actors;

private:
	Mesh* mesh;

};

#endif