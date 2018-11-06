#include "scenenode.h"
#include "scene.h"

SceneNode::SceneNode() {

	scene = nullptr;
	sceneSet = false;

}

void SceneNode::Add(SceneNode* node) {

	if (sceneSet) {
		node->AddToScene(scene);
	}

	childNodes.push_back(node);

}

void SceneNode::Remove(SceneNode* node) {

	if (sceneSet) {
		node->RemoveFromScene();
	}

	for (auto iterator = childNodes.begin(); iterator != childNodes.end(); iterator++) {

		if (*iterator == node) {
			childNodes.erase(iterator);
			return;
		}

	}

}

void SceneNode::Add(Actor* actor) {

	if (scene != nullptr) {
		scene->Add(actor);
	}

	actors.push_back(actor);

}

void SceneNode::Remove(Actor* actor) {

	if (scene != nullptr) {
		scene->Remove(actor);
	}

	for (auto iterator = actors.begin(); iterator != actors.end(); iterator++) {

		if (*iterator == actor) {
			actors.erase(iterator);
			return;
		}

	}

}

void SceneNode::Add(Light* light) {

	if (scene != nullptr) {
		scene->Add(light);
	}

	lights.push_back(light);

}

void SceneNode::Remove(Light* light) {

	if (scene != nullptr) {
		scene->Remove(light);
	}

	for (auto iterator = lights.begin(); iterator != lights.end(); iterator++) {

		if (*iterator == light) {
			lights.erase(iterator);
			return;
		}

	}

}

void SceneNode::Update(mat4 parentTransformation) {

	mat4 transformation = parentTransformation * transformationMatrix;

	for (SceneNode* node : childNodes) {
		node->Update(transformation);
	}

	for (Actor* actor : actors) {
		actor->transformedMatrix = transformation * actor->transformedMatrix;
	}

	// Lights should be calculated here

}

void SceneNode::AddToScene(Scene* scene) {

	if (sceneSet)
		return;

	for (SceneNode* node : childNodes) {
		node->AddToScene(scene);
	}

	for (Actor* actor : actors) {
		scene->Add(actor);
	}

	for (Light* light : lights) {
		scene->Add(light);
	}

	this->scene = scene;
	sceneSet = true;

}

void SceneNode::RemoveFromScene() {

	if (!sceneSet)
		return;

	for (SceneNode* node : childNodes) {
		node->RemoveFromScene();
	}

	for (Actor* actor : actors) {
		scene->Remove(actor);
	}

	for (Light* light : lights) {
		scene->Remove(light);
	}

	this->scene = nullptr;
	sceneSet = false;

}

vector<SceneNode*> SceneNode::GetChildNodes() {

	return childNodes;

}

vector<Actor*> SceneNode::GetActors() {

	return actors;

}

vector<Light*> SceneNode::GetLights() {

	return lights;

}