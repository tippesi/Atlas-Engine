#include "ActorBatch.h"

ActorBatch::ActorBatch(Mesh* mesh) : mesh(mesh) {

	changed = false;

}

void ActorBatch::Add(Actor* actor) {

	actors.push_back(actor);
	changed = true;

}

void ActorBatch::Remove(Actor* actor) {

	for (auto iterator = actors.begin(); iterator != actors.end(); iterator++) {

		if (*iterator == actor) {
			actors.erase(iterator);
			changed = true;
			return;
		}

	}

}

Mesh* ActorBatch::GetMesh() {

	return mesh;

}

int32_t ActorBatch::GetSize() {

	return (uint32_t)actors.size();

}

void ActorBatch::ClearContent() {

	actors.clear();

}

void ActorBatch::DeleteContent() {

	for (auto& actor : actors) {
		delete actor;
	}

	ClearContent();

}