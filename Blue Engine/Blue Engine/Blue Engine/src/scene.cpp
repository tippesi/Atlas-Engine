#include "scene.h"

Scene::Scene() {

	rootNode->AddToScene(this);

}

void Scene::Add(Actor* actor) {

	for (ActorBatch* actorBatch : actorBatches) {

		if (actorBatch->GetMesh() == actor->mesh) {
			actorBatch->Add(actor);
			return;
		}

	}

}

void Scene::Remove(Actor* actor) {

	for (ActorBatch* actorBatch : actorBatches) {

		if (actorBatch->GetMesh() == actor->mesh) {
			actorBatch->Remove(actor);
			return;
		}

	}

}

void Scene::Add(Light* light) {

	lights.push_back(light);

}

void Scene::Remove(Light* light) {

	for (auto iterator = lights.begin(); iterator != lights.end(); iterator++) {

		if (*iterator == light) {
			lights.erase(iterator);
			return;
		}

	}

}

void Scene::Update() {

	rootNode->Update(mat4(1.0f));

}