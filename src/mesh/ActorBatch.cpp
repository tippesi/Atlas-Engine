#include "ActorBatch.h"

ActorBatch::ActorBatch(Mesh* mesh) : mesh(mesh) {



}

void ActorBatch::Add(Actor* actor) {

	actors.push_back(actor);

}

void ActorBatch::Remove(Actor* actor) {

	for (auto iterator = actors.begin(); iterator != actors.end(); iterator++) {

		if (*iterator == actor) {
			actors.erase(iterator);
			return;
		}

	}

}

Mesh* ActorBatch::GetMesh() {

	return mesh;

}

int32_t ActorBatch::GetSize() {

	return actors.size();

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