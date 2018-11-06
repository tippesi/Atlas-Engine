#include "scene.h"

Scene::Scene() {

	rootNode = new SceneNode();
	rootNode->AddToScene(this);

	sky = new Sky();
	postProcessing = new PostProcessing();

}

void Scene::Add(Actor* actor) {

	for (ActorBatch* actorBatch : actorBatches) {

		if (actorBatch->GetMesh() == actor->mesh) {
			actorBatch->Add(actor);
			return;
		}

	}

	ActorBatch* actorBatch = new ActorBatch(actor->mesh);
	actorBatch->Add(actor);

	actorBatches.push_back(actorBatch);

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

	// We have to copy the matrices because some actors might not be added
	// to the root node. In this case the trasformedMatrix would never get updated
	for (auto actorBatch : actorBatches) {
		for (auto actor : actorBatch->actors) {
			actor->transformedMatrix = actor->modelMatrix;
		}
	}

	rootNode->Update(mat4(1.0f));

}

Scene::~Scene() {

	delete rootNode;
	delete sky;
	delete postProcessing;

}