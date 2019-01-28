#ifndef MESHACTORBATCH_H
#define MESHACTORBATCH_H

#include "../System.h"
#include "MeshActor.h"
#include "Mesh.h"

#include <vector>

/**
 * Manages the ordering of mesh actors
 */
class MeshActorBatch {

public:
	/**
	 * Constructs an MeshActorBatch object for a mesh
	 * @param mesh A pointer to a Mesh object
	 */
	MeshActorBatch(Mesh* mesh);

	/**
	 * Adds a mesh actor to the batch
	 * @param actor A pointer to an MeshActor object
	 */
	void Add(MeshActor* actor);

	/**
	 * Removes a mesh actor from the actor batch
	 * @param actor A pointer to an MeshActor object
	 */
	void Remove(MeshActor* actor);

	/**
	 * Returns the mesh of the batch
	 * @return A pointer to a Mesh object
	 */
	Mesh* GetMesh();


	/**
	 * Returns the number of mesh actors in the batch
	 * @return An integer with the number of mesh actors
	 */
	int32_t GetSize();

	/**
	 * Removes all mesh actors from the batch.
	 */
	void Clear();

	vector<MeshActor*> actors;

private:
	Mesh* const mesh;

	bool changed;

};

#endif