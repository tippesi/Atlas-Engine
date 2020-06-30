#include "SceneNode.h"
#include "Scene.h"

#include "../audio/AudioManager.h"

#include <algorithm>

namespace Atlas {

	namespace Scene {

		SceneNode::SceneNode(const SceneNode& that) {

			DeepCopy(that);

		}

		SceneNode& SceneNode::operator=(const SceneNode& that) {

			if (this != &that) {

				DeepCopy(that);

			}

			return *this;

		}

		void SceneNode::Add(SceneNode *node) {

			if (sceneSet) {
				node->AddToScene(spacePartitioning, meshMap);
			}

			childNodes.push_back(node);

		}

		void SceneNode::Remove(SceneNode *node) {

			if (sceneSet) {
				node->RemoveFromScene();
			}

			auto item = std::find(childNodes.begin(), childNodes.end(), node);

			if (item != childNodes.end()) {
				childNodes.erase(item);
			}

		}

		void SceneNode::Add(Actor::MovableMeshActor *actor) {

			movableMeshActors.push_back(actor);

			AddInternal(actor);

		}

		void SceneNode::Remove(Actor::MovableMeshActor *actor) {

			if (sceneSet) {
				spacePartitioning->Remove(actor);
			}

			auto item = std::find(movableMeshActors.begin(), movableMeshActors.end(), actor);

			if (item != movableMeshActors.end()) {
				movableMeshActors.erase(item);
			}

			RemoveInternal(actor);

		}

		void SceneNode::Add(Actor::StaticMeshActor *actor) {

			addableStaticMeshActors.push_back(actor);

			AddInternal(actor);

		}

		void SceneNode::Remove(Actor::StaticMeshActor *actor) {

			if (sceneSet) {
				spacePartitioning->Remove(actor);
			}

			auto item = std::find(staticMeshActors.begin(), staticMeshActors.end(), actor);

			if (item != staticMeshActors.end()) {
				staticMeshActors.erase(item);
			}

			RemoveInternal(actor);

		}

		void SceneNode::Add(Actor::DecalActor *actor) {

			decalActors.push_back(actor);

		}

		void SceneNode::Remove(Actor::DecalActor *actor) {

			if (sceneSet) {
				spacePartitioning->Remove(actor);
			}

			auto item = std::find(decalActors.begin(), decalActors.end(), actor);

			if (item != decalActors.end()) {
				decalActors.erase(item);
			}

		}

		void SceneNode::Add(Actor::AudioActor* actor) {

			audioActors.push_back(actor);

			Atlas::Audio::AudioManager::AddMusic(actor);

		}

		void SceneNode::Remove(Actor::AudioActor* actor) {

			if (sceneSet) {
				spacePartitioning->Remove(actor);
			}

			auto item = std::find(audioActors.begin(), audioActors.end(), actor);

			if (item != audioActors.end()) {
				audioActors.erase(item);
			}

			Atlas::Audio::AudioManager::RemoveMusic(actor);

		}

		void SceneNode::Add(Lighting::Light *light)  {

			if (sceneSet) {
				spacePartitioning->Add(light);
			}

			lights.push_back(light);

		}

		void SceneNode::Remove(Lighting::Light *light) {

			if (sceneSet) {
				spacePartitioning->Remove(light);
			}

			auto item = std::find(lights.begin(), lights.end(), light);

			if (item != lights.end()) {
				lights.erase(item);
			}

		}

		void SceneNode::SetMatrix(mat4 matrix) {

			this->matrix = matrix;

			matrixChanged = true;

		}

		mat4 SceneNode::GetMatrix() const {

			return matrix;

		}

		mat4 SceneNode::GetGlobalMatrix() const {

			return globalMatrix;

		}

		void SceneNode::Clear() {

			childNodes.clear();

			movableMeshActors.clear();
			staticMeshActors.clear();
			decalActors.clear();
			audioActors.clear();
			lights.clear();

		}

		std::vector<Actor::MovableMeshActor*> SceneNode::GetNodeMovableMeshActors() {

			return movableMeshActors;

		}

		std::vector<Actor::StaticMeshActor*> SceneNode::GetNodeStaticMeshActors() {

			return staticMeshActors;

		}

		std::vector<Actor::DecalActor*> SceneNode::GetNodeDecalActors() {

			return decalActors;

		}

		std::vector<Actor::AudioActor*> SceneNode::GetNodeAudioActors() {

			return audioActors;

		}

		std::vector<Lighting::Light*> SceneNode::GetNodeLights() {

			return lights;

		}

		std::vector<SceneNode*> SceneNode::GetNodeChildren() {

			return childNodes;

		}


		bool SceneNode::Update(Camera* camera, float deltaTime, mat4 parentTransformation,
			bool parentTransformChanged) {

			bool changed = false;

			parentTransformChanged |= matrixChanged;

			changed |= parentTransformChanged;

			bool removed = false;

			if (matrixChanged) {
				globalMatrix = parentTransformation * matrix;
				matrixChanged = false;
			}

			for (auto &node : childNodes) {
				changed |= node->Update(camera, deltaTime, globalMatrix, parentTransformChanged);
			}

			// Only update the static mesh actors if the node moves (the static actors can't
			// be moved after being initialized by their constructor)
			if (parentTransformChanged) {

			    for (auto &meshActor : staticMeshActors) {

					spacePartitioning->Remove(meshActor);
			        meshActor->Update(*camera, deltaTime,
						parentTransformation, true);
					spacePartitioning->Add(meshActor);

			    }

			}

			for (auto& meshActor : addableStaticMeshActors) {

				staticMeshActors.push_back(meshActor);
				meshActor->Update(*camera, deltaTime,
					parentTransformation, true);
				// Because they won't be updated we need to do this right here
				meshActor->lastGlobalMatrix = meshActor->globalMatrix;
				spacePartitioning->Add(meshActor);

				changed = true;

			}

			addableStaticMeshActors.clear();

			for (auto &meshActor : movableMeshActors) {

				if (meshActor->HasMatrixChanged() || parentTransformChanged) {
					spacePartitioning->Remove(meshActor);
					removed = true;
					changed = true;
				}

				meshActor->Update(*camera, deltaTime, 
					globalMatrix, parentTransformChanged);

				if (removed) {
					spacePartitioning->Add(meshActor);
					removed = false;
				}

			}

			for (auto &decalActor : decalActors) {
				if (decalActor->HasMatrixChanged() || parentTransformChanged) {
					spacePartitioning->Remove(decalActor);
					removed = true;
				}

				decalActor->Update(*camera, deltaTime, 
					globalMatrix, parentTransformChanged);

				if (removed) {
					spacePartitioning->Add(decalActor);
					removed = false;
				}
			}

			for (auto& audioActor : audioActors) {
				if (audioActor->HasMatrixChanged() || parentTransformChanged) {
					spacePartitioning->Remove(audioActor);
					removed = true;
				}

				audioActor->Update(*camera, deltaTime,
					globalMatrix, parentTransformChanged);

				if (removed) {
					spacePartitioning->Add(audioActor);
					removed = false;
				}
			}

			for (auto &light : lights) {
				light->Update(camera);
			}

			return changed;

		}

		void SceneNode::AddToScene(SpacePartitioning* spacePartitioning,
			std::unordered_map<Mesh::Mesh*, int32_t>* meshMap) {

			if (sceneSet)
				return;

			for (auto &node : childNodes) {
				node->AddToScene(spacePartitioning, meshMap);
			}

			for (auto& meshActor : movableMeshActors) {
				AddInternal(meshActor);
			}

			for (auto& meshActor : staticMeshActors) {
				AddInternal(meshActor);
			}

			for (auto& light : lights) {
				spacePartitioning->Add(light);
			}

			this->spacePartitioning = spacePartitioning;
			this->meshMap = meshMap;

			sceneSet = true;
			matrixChanged = true;

		}

		void SceneNode::RemoveFromScene() {

			if (!sceneSet)
				return;

			for (auto &node : childNodes) {
				node->RemoveFromScene();
			}

			for (auto &meshActor : movableMeshActors) {
				spacePartitioning->Remove(meshActor);
				RemoveInternal(meshActor);
			}

			for (auto &meshActor : staticMeshActors) {
				spacePartitioning->Remove(meshActor);
				RemoveInternal(meshActor);
			}

			for (auto &decalActor : decalActors) {
				spacePartitioning->Remove(decalActor);
			}

			sceneSet = false;

		}

		void SceneNode::DeepCopy(const SceneNode& that) {

			RemoveFromScene();

			matrixChanged = true;

			matrix = that.matrix;
			globalMatrix = that.globalMatrix;

			movableMeshActors = that.movableMeshActors;
			staticMeshActors = that.staticMeshActors;
			decalActors = that.decalActors;
			lights = that.lights;

			childNodes.resize(that.childNodes.size());

			for (size_t i = 0; i < that.childNodes.size(); i++)
				childNodes[i] = new SceneNode(*that.childNodes[i]);

			if (spacePartitioning)
				AddToScene(spacePartitioning, meshMap);

		}

		void SceneNode::AddInternal(Actor::MeshActor* actor) {

			if (!sceneSet)
				return;

			auto res = meshMap->find(actor->mesh);

			if (res == meshMap->end()) {
				(*meshMap)[actor->mesh] = 1;
			}
			else {
				res->second++;
			}

		}

		void SceneNode::RemoveInternal(Actor::MeshActor* actor) {

			if (!sceneSet)
				return;

			auto res = meshMap->find(actor->mesh);

			if (res == meshMap->end())
				return;

			res->second--;

			if (!res->second) {
				meshMap->erase(res->first);
			}

		}

	}

}