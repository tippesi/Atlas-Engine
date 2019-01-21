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

void SceneNode::Add(MeshActor* actor) {

	if (scene != nullptr) {
		scene->Add(actor);
	}

	meshActors.push_back(actor);

}

void SceneNode::Remove(MeshActor* actor) {

	if (scene != nullptr) {
		scene->Remove(actor);
	}

	for (auto iterator = meshActors.begin(); iterator != meshActors.end(); iterator++) {

		if (*iterator == actor) {
			meshActors.erase(iterator);
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

	for (auto& meshActor : meshActors) {
		meshActor->transformedMatrix = transformation * meshActor->transformedMatrix;
	}

	// Lights should be calculated here

}

void SceneNode::AddToScene(Scene* scene) {

	if (sceneSet)
		return;

	for (auto& node : childNodes) {
		node->AddToScene(scene);
	}

	for (auto& meshActor : meshActors) {
		scene->Add(meshActor);
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

	for (auto& meshActor : meshActors) {
		scene->Remove(meshActor);
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

vector<MeshActor*> SceneNode::GetMeshActors() {

	return meshActors;

}

vector<ILight*> SceneNode::GetLights() {

	return lights;

}