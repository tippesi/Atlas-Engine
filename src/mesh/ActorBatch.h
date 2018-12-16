#ifndef ACTORBATCH_H
#define ACTORBATCH_H

#include "../System.h"
#include "Actor.h"
#include "Mesh.h"

#include <vector>

/**
 * Manages the ordering of actors
 */
class ActorBatch {

public:
	/**
	 * Constructs an ActorBatch object for a mesh
	 * @param mesh A pointer to a Mesh object
	 */
	ActorBatch(Mesh* mesh);

	/**
	 * Adds an actor to the actor batch
	 * @param actor A pointer to an Actor object
	 */
	void Add(Actor* actor);

	/**
	 * Removes an actor from the actor batch
	 * @param actor A pointer to an Actor object
	 */
	void Remove(Actor* actor);

	/**
	 * Returns the mesh of the batch
	 * @return A pointer to a Mesh object
	 */
	Mesh* GetMesh();


	/**
	 * Returns the number of actors in the batch
	 * @return An integer with the number of actors
	 */
	int32_t GetSize();

	/**
	 *
	 */
	void ClearContent();

	/**
	 *
	 */
	void DeleteContent();

	ActorBatch::~ActorBatch();

	vector<Actor*> actors;

private:
	Mesh* mesh;

	bool changed;

};

#endif