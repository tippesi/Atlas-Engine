#include "SceneNode.h"
#include "Scene.h"

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

void SceneNode::Add(ILight* light) {

	if (scene != nullptr) {
		scene->Add(light);
	}

	lights.push_back(light);

}

void SceneNode::Remove(ILight* light) {

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

	for (auto& node : childNodes) {
		node->Update(transformation);
	}

	for (auto& actor : actors) {
		actor->transformedMatrix = transformation * actor->transformedMatrix;
	}

	// Lights should be calculated here

}

void SceneNode::AddToScene(Scene* scene) {

	if (sceneSet)
		return;

	for (auto& node : childNodes) {
		node->AddToScene(scene);
	}

	for (auto& actor : actors) {
		scene->Add(actor);
	}

	for (auto& light : lights) {
		scene->Add(light);
	}

	this->scene = scene;
	sceneSet = true;

}

void SceneNode::RemoveFromScene() {

	if (!sceneSet)
		return;

	for (auto& node : childNodes) {
		node->RemoveFromScene();
	}

	for (auto& actor : actors) {
		scene->Remove(actor);
	}

	for (auto& light : lights) {
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

vector<ILight*> SceneNode::GetLights() {

	return lights;

}