#include "SceneNode.h"
#include "Scene.h"

namespace Atlas {

	namespace Scene {

		SceneNode::SceneNode() {

			sceneSet = false;

		}

		void SceneNode::Add(SceneNode *node) {

			if (sceneSet) {
				node->AddToScene(meshActorOctree, decalActorOctree);
			}

			childNodes.push_back(node);

		}

		void SceneNode::Remove(SceneNode *node) {

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

		void SceneNode::Add(Actor::MeshActor *actor) {

			meshActors.push_back(actor);

		}

		void SceneNode::Remove(Actor::MeshActor *actor) {

			if (sceneSet) {
				meshActorOctree->Remove(actor, actor->aabb);
			}

			for (auto iterator = meshActors.begin(); iterator != meshActors.end(); iterator++) {

				if (*iterator == actor) {
					meshActors.erase(iterator);
					return;
				}

			}

		}

		void SceneNode::Add(Actor::DecalActor *actor) {

			decalActors.push_back(actor);

		}

		void SceneNode::Remove(Actor::DecalActor *actor) {

			if (sceneSet) {
				decalActorOctree->Remove(actor, actor->aabb);
			}

			for (auto iterator = decalActors.begin(); iterator != decalActors.end(); iterator++) {

				if (*iterator == actor) {
					decalActors.erase(iterator);
					return;
				}

			}

		}

		void SceneNode::Add(Lighting::Light *light) {

			lights.push_back(light);

		}

		void SceneNode::Remove(Lighting::Light *light) {

			for (auto iterator = lights.begin(); iterator != lights.end(); iterator++) {

				if (*iterator == light) {
					lights.erase(iterator);
					return;
				}

			}

		}

		void SceneNode::SetMatrix(mat4 matrix) {

			matrix = matrix;

			matrixChanged = true;

		}

		void SceneNode::Clear() {

			childNodes.clear();

			meshActors.clear();
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

			if (!update) {
				
				for (auto &meshActor : meshActors) {
					if (meshActor->HasMatrixChanged() || parentTransformChanged) {
						update = true;
						meshActorOctree->Remove(meshActor, meshActor->aabb);
						removed = true;
						// meshActor->Update(deltaTime, transformedMatrix, parentTransformChanged);
					}
					meshActor->Update(deltaTime, transformedMatrix, parentTransformChanged);
					if (removed) {
						meshActorOctree->Insert(meshActor, meshActor->aabb);
						removed = false;
					}
				}
			}

			for (auto &decalActor : decalActors) {
				if (decalActor->HasMatrixChanged() || parentTransformChanged) {
					decalActorOctree->Remove(decalActor, decalActor->aabb);
					removed = true;
				}
				decalActor->Update(deltaTime, transformedMatrix, parentTransformChanged);
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

		void SceneNode::AddToScene(Common::Octree<Actor::MeshActor*>* meshActorOctree,
			Common::Octree<Actor::DecalActor*>* decalActorOctree) {

			if (sceneSet)
				return;

			for (auto &node : childNodes) {
				node->AddToScene(meshActorOctree, decalActorOctree);
			}

			for (auto &meshActor : meshActors) {
				meshActorOctree->Insert(meshActor, meshActor->aabb);
			}

			for (auto &decalActor : decalActors) {
				decalActorOctree->Insert(decalActor, decalActor->aabb);
			}

			this->meshActorOctree = meshActorOctree;
			this->decalActorOctree = decalActorOctree;

			sceneSet = true;

		}

		void SceneNode::RemoveFromScene() {

			if (!sceneSet)
				return;

			for (auto &node : childNodes) {
				node->RemoveFromScene();
			}

			for (auto &meshActor : meshActors) {
				meshActorOctree->Remove(meshActor, meshActor->aabb);
			}

			for (auto &decalActor : decalActors) {
				decalActorOctree->Remove(decalActor, decalActor->aabb);
			}

			sceneSet = false;

		}

	}

}