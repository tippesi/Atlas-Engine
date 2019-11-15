#include "SceneNode.h"
#include "Scene.h"

#include "../audio/AudioManager.h"

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
				node->AddToScene(spacePartitioning);
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

		}

		void SceneNode::Remove(Actor::MovableMeshActor *actor) {

			if (sceneSet) {
				spacePartitioning->Remove(actor);
			}

			auto item = std::find(movableMeshActors.begin(), movableMeshActors.end(), actor);

			if (item != movableMeshActors.end()) {
				movableMeshActors.erase(item);
			}

		}

		void SceneNode::Add(Actor::StaticMeshActor *actor) {

			addableStaticMeshActors.push_back(actor);

		}

		void SceneNode::Remove(Actor::StaticMeshActor *actor) {

			if (sceneSet) {
				spacePartitioning->Remove(actor);
			}

			auto item = std::find(staticMeshActors.begin(), staticMeshActors.end(), actor);

			if (item != staticMeshActors.end()) {
				staticMeshActors.erase(item);
			}

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

			matrix = matrix;

			matrixChanged = true;

		}

		void SceneNode::Clear() {

			childNodes.clear();

			movableMeshActors.clear();
			staticMeshActors.clear();
			decalActors.clear();
			lights.clear();

		}

		bool SceneNode::Update(Camera* camera, float deltaTime, mat4 parentTransformation,
			bool parentTransformChanged) {

			bool changed = false;

			parentTransformChanged |= matrixChanged;

			changed |= parentTransformChanged;

			bool removed = false;

			if (matrixChanged) {
				transformedMatrix = parentTransformation * matrix;
				matrixChanged = false;
			}

			for (auto &node : childNodes) {
				changed |= node->Update(camera, deltaTime, transformedMatrix, parentTransformChanged);
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
					transformedMatrix, parentTransformChanged);

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
					transformedMatrix, parentTransformChanged);

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
					transformedMatrix, parentTransformChanged);

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

		void SceneNode::AddToScene(SpacePartitioning* spacePartitioning) {

			if (sceneSet)
				return;

			for (auto &node : childNodes) {
				node->AddToScene(spacePartitioning);
			}

			for (auto& light : lights) {
				spacePartitioning->Add(light);
			}

			this->spacePartitioning = spacePartitioning;

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
			}

			for (auto &meshActor : staticMeshActors) {
				spacePartitioning->Remove(meshActor);
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
			transformedMatrix = that.transformedMatrix;

			movableMeshActors = that.movableMeshActors;
			staticMeshActors = that.staticMeshActors;
			decalActors = that.decalActors;
			lights = that.lights;

			childNodes.resize(that.childNodes.size());

			for (size_t i = 0; i < that.childNodes.size(); i++)
				childNodes[i] = new SceneNode(*that.childNodes[i]);

			if (spacePartitioning)
				AddToScene(spacePartitioning);

		}

	}

}