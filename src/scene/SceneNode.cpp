#include "SceneNode.h"
#include "Scene.h"

namespace Atlas {

	namespace Scene {

		SceneNode::SceneNode() {

			sceneSet = false;

		}

		void SceneNode::Add(SceneNode *node) {

			if (sceneSet) {
				node->AddToScene(movableMeshActorOctree, staticMeshActorOctree, decalActorOctree);
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
				movableMeshActorOctree->Remove(actor, actor->aabb);
			}

			auto item = std::find(movableMeshActors.begin(), movableMeshActors.end(), actor);

			if (item != movableMeshActors.end()) {
				movableMeshActors.erase(item);
			}

		}

		void SceneNode::Add(Actor::StaticMeshActor *actor) {

			staticMeshActors.push_back(actor);

		}

		void SceneNode::Remove(Actor::StaticMeshActor *actor) {

			if (sceneSet) {
				staticMeshActorOctree->Remove(actor, actor->aabb);
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
				decalActorOctree->Remove(actor, actor->aabb);
			}

			auto item = std::find(decalActors.begin(), decalActors.end(), actor);

			if (item != decalActors.end()) {
				decalActors.erase(item);
			}

		}

		void SceneNode::Add(Lighting::Light *light) {

			lights.push_back(light);

		}

		void SceneNode::Remove(Lighting::Light *light) {

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

		void SceneNode::Update(Camera* camera, float deltaTime, std::vector<Lighting::Light*>& lights,
				mat4 parentTransformation, bool parentTransformChanged) {

			parentTransformChanged |= matrixChanged;

			bool removed = false;

			if (matrixChanged) {
				transformedMatrix = parentTransformation * matrix;
				matrixChanged = false;
			}

			for (auto &node : childNodes) {
				node->Update(camera, deltaTime, lights, transformedMatrix, parentTransformChanged);
			}

			// Only update the static mesh actors if the node moves (the static actors can't
			// be moved after being initialized by their constructor)
			if (parentTransformChanged) {

			    staticMeshActorOctree->Clear();

			    for (auto &meshActor : staticMeshActors) {

			        meshActor->Update(*camera, deltaTime,
						parentTransformation, true);
			        staticMeshActorOctree->Insert(meshActor, meshActor->aabb);

			    }

			}

			for (auto &meshActor : movableMeshActors) {

				if (meshActor->HasMatrixChanged() || parentTransformChanged) {
					movableMeshActorOctree->Remove(meshActor, meshActor->aabb);
					removed = true;
				}

				meshActor->Update(*camera, deltaTime, 
					transformedMatrix, parentTransformChanged);

				if (removed) {
					movableMeshActorOctree->Insert(meshActor, meshActor->aabb);
					removed = false;
				}

			}

			for (auto &decalActor : decalActors) {
				if (decalActor->HasMatrixChanged() || parentTransformChanged) {
					decalActorOctree->Remove(decalActor, decalActor->aabb);
					removed = true;
				}

				decalActor->Update(*camera, deltaTime, 
					transformedMatrix, parentTransformChanged);

				if (removed) {
					decalActorOctree->Insert(decalActor, decalActor->aabb);
					removed = false;
				}
			}

			for (auto &light : this->lights) {
				light->Update(camera);
				lights.push_back(light);
			}

		}

		void SceneNode::AddToScene(Common::Octree<Actor::MovableMeshActor*>* movableMeshActorOctree,
				Common::Octree<Actor::StaticMeshActor*>* staticMeshActorOctree,
				Common::Octree<Actor::DecalActor*>* decalActorOctree) {

			if (sceneSet)
				return;

			for (auto &node : childNodes) {
				node->AddToScene(movableMeshActorOctree, staticMeshActorOctree, decalActorOctree);
			}

			this->movableMeshActorOctree = movableMeshActorOctree;
			this->staticMeshActorOctree = staticMeshActorOctree;
			this->decalActorOctree = decalActorOctree;

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
				movableMeshActorOctree->Remove(meshActor, meshActor->aabb);
			}

			for (auto &meshActor : staticMeshActors) {
				staticMeshActorOctree->Remove(meshActor, meshActor->aabb);
			}

			for (auto &decalActor : decalActors) {
				decalActorOctree->Remove(decalActor, decalActor->aabb);
			}

			sceneSet = false;

		}

	}

}