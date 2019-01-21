#include "MeshActorBatch.h"

MeshActorBatch::MeshActorBatch(Mesh* mesh) : mesh(mesh) {

	changed = false;

}

void MeshActorBatch::Add(MeshActor* actor) {

	actors.push_back(actor);
	changed = true;

}

void MeshActorBatch::Remove(MeshActor* actor) {

	for (auto iterator = actors.begin(); iterator != actors.end(); iterator++) {

		if (*iterator == actor) {
			actors.erase(iterator);
			changed = true;
			return;
		}

	}

}

Mesh* MeshActorBatch::GetMesh() {

	return mesh;

}

int32_t MeshActorBatch::GetSize() {

	return (uint32_t)actors.size();

}

void MeshActorBatch::Clear() {

	actors.clear();

}