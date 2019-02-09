#include "Scene.h"
#include "FrustumCulling.h"

namespace Atlas {

	Scene::Scene() {

		rootNode = new SceneNode();
		rootNode->AddToScene(this);

		sky = new Lighting::Sky();
		postProcessing = new PostProcessing::PostProcessing();

		renderList = new RenderList(AE_GEOMETRY_RENDERLIST, 0);

	}

	Scene::~Scene() {

		delete rootNode;
		delete sky;
		delete postProcessing;

	}

	void Scene::Add(Mesh::MeshActor *actor) {

		for (auto &meshActorBatch : meshActorBatches) {

			if (meshActorBatch->GetMesh() == actor->mesh) {
				meshActorBatch->Add(actor);
				renderList->Add(actor);
				return;
			}

		}

		auto meshActorBatch = new Mesh::MeshActorBatch(actor->mesh);
		meshActorBatch->Add(actor);

		meshActorBatches.push_back(meshActorBatch);

		renderList->Add(actor);

	}

	void Scene::Remove(Mesh::MeshActor *actor) {

		for (auto &meshActorBatch : meshActorBatches) {

			if (meshActorBatch->GetMesh() == actor->mesh) {
				meshActorBatch->Remove(actor);
				return;
			}

		}

	}

	void Scene::Add(Terrain::Terrain *terrain) {

		terrains.push_back(terrain);

	}

	void Scene::Remove(Terrain::Terrain *terrain) {

		for (auto iterator = terrains.begin(); iterator != terrains.end(); iterator++) {

			if (*iterator == terrain) {
				terrains.erase(iterator);
				return;
			}

		}

	}

	void Scene::Add(Lighting::Light *light) {

		lights.push_back(light);

		renderList->Add(light);

	}

	void Scene::Remove(Lighting::Light *light) {

		for (auto iterator = lights.begin(); iterator != lights.end(); iterator++) {

			if (*iterator == light) {
				lights.erase(iterator);
				return;
			}

		}

	}

	void Scene::Add(Decal *decal) {

		decals.push_back(decal);

		// renderList->Add(light);

	}

	void Scene::Remove(Decal *decal) {

		for (auto iterator = decals.begin(); iterator != decals.end(); iterator++) {

			if (*iterator == decal) {
				decals.erase(iterator);
				return;
			}

		}

	}

	void Scene::Update(Camera *camera) {

		// We have to copy the matrices because some actors might not be added
		// to the root node. In this case the trasformedMatrix would never get updated
		for (auto &meshActorBatch : meshActorBatches) {
			for (auto &actor : meshActorBatch->actors) {
				actor->transformedMatrix = actor->modelMatrix;
			}
		}

		for (auto &light : lights) {
			light->Update(camera);
		}

		rootNode->Update(mat4(1.0f));

		// renderList->Clear();

		FrustumCulling::CullActorsFromScene(this, camera);

		FrustumCulling::CullLightsFromScene(this, camera);

	}

}