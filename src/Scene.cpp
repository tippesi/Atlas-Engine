#include "Scene.h"
#include "FrustumCulling.h"

Scene::Scene() {

	rootNode = new SceneNode();
	rootNode->AddToScene(this);

	sky = new Sky();
	postProcessing = new PostProcessing();

	renderList = new RenderList(GEOMETRY_RENDERLIST, 0);

}

void Scene::Add(Actor* actor) {

	for (ActorBatch*& actorBatch : actorBatches) {

		if (actorBatch->GetMesh() == actor->mesh) {
			actorBatch->Add(actor);
			return;
		}

	}

	ActorBatch* actorBatch = new ActorBatch(actor->mesh);
	actorBatch->Add(actor);

	actorBatches.push_back(actorBatch);

	renderList->Add(actor);

}

void Scene::Remove(Actor* actor) {

	for (ActorBatch*& actorBatch : actorBatches) {

		if (actorBatch->GetMesh() == actor->mesh) {
			actorBatch->Remove(actor);
			return;
		}

	}

}

void Scene::Add(Terrain* terrain) {

	terrains.push_back(terrain);

}

void Scene::Remove(Terrain* terrain) {

	for (auto iterator = terrains.begin(); iterator != terrains.end(); iterator++) {

		if (*iterator == terrain) {
			terrains.erase(iterator);
			return;
		}

	}

}

void Scene::Add(ILight* light) {

	lights.push_back(light);

	renderList->Add(light);

}

void Scene::Remove(ILight* light) {

	for (auto iterator = lights.begin(); iterator != lights.end(); iterator++) {

		if (*iterator == light) {
			lights.erase(iterator);
			return;
		}

	}

}

void Scene::Update(Camera* camera) {

	// We have to copy the matrices because some actors might not be added
	// to the root node. In this case the trasformedMatrix would never get updated
	for (auto actorBatch : actorBatches) {
		for (auto actor : actorBatch->actors) {
			actor->transformedMatrix = actor->modelMatrix;
		}
	}

	for (auto& light : lights) {
		light->Update(camera);
	}

	rootNode->Update(mat4(1.0f));

	renderList->Clear();

	FrustumCulling::CullActorsFromScene(this, camera);

	FrustumCulling::CullLightsFromScene(this, camera);

}

Scene::~Scene() {

	delete rootNode;
	delete sky;
	delete postProcessing;

}