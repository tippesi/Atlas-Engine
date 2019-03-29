#include "Scene.h"
#include "FrustumCulling.h"

namespace Atlas {

	namespace Scene {

		Scene::Scene() : SceneNode() {

			renderList = RenderList(AE_OPAQUE_RENDERLIST);

			Common::AABB aabb(vec3(-2048.0f), vec3(2048.0f));

			movableMeshOctree = Common::Octree<Actor::MovableMeshActor*>(aabb, 8);
			staticMeshOctree = Common::Octree<Actor::StaticMeshActor*>(aabb, 8);
			decalOctree = Common::Octree<Actor::DecalActor*>(aabb, 8);

			AddToScene(&movableMeshOctree, &staticMeshOctree, &decalOctree);

		}

		Scene::~Scene() {



		}

		void Scene::Add(Terrain::Terrain *terrain) {

			terrains.push_back(terrain);

		}

		void Scene::Remove(Terrain::Terrain *terrain) {

			auto item = std::find(terrains.begin(), terrains.end(), terrain);

			if (item != terrains.end()) {
				terrains.erase(item);
			}

		}

		void Scene::Update(Camera *camera, float deltaTime) {

			renderList.Clear();

			for (auto &terrain : terrains) {
				terrain->Update(camera);
			}

			auto accumulatedLights = std::vector<Lighting::Light*>();

			SceneNode::Update(camera, deltaTime, accumulatedLights, mat4(1.0f), false);

			std::vector<Actor::MovableMeshActor*> movableMeshActorQuery;
			std::vector<Actor::StaticMeshActor*> staticMeshActorQuery;

			for (auto &light : accumulatedLights) {
				// We always want to render all lights
				renderList.Add(light);
				// Check if the renderlist of the shadow needs an update
				if (light->GetShadow()) {
					if (!light->GetShadow()->update)
						continue;

					for (auto& component : light->GetShadow()->components) {
						component.renderList->Clear();
						movableMeshActorQuery.clear();
						staticMeshActorQuery.clear();

						mat4 inverseMatrix = glm::inverse(component.projectionMatrix * component.viewMatrix);

						Common::AABB base(vec3(-1.0f), vec3(1.0f));

						auto aabb = base.Transform(inverseMatrix);

						movableMeshOctree.QueryAABB(movableMeshActorQuery, aabb);

						for (auto &meshActor : movableMeshActorQuery) {
							component.renderList->Add(meshActor);
						}

						staticMeshOctree.QueryAABB(staticMeshActorQuery, aabb);

						for (auto &meshActor : staticMeshActorQuery) {
							component.renderList->Add(meshActor);
						}

					}
				}
			}

			movableMeshActorQuery.clear();
			staticMeshActorQuery.clear();

			mat4 inverseMatrix = glm::inverse(camera->projectionMatrix * camera->viewMatrix);

			Common::AABB base(vec3(-1.0f), vec3(1.0f));

            auto aabb = base.Transform(glm::inverse(camera->projectionMatrix * camera->viewMatrix));

			movableMeshOctree.QueryAABB(movableMeshActorQuery, aabb);

			for (auto& actor : movableMeshActorQuery) {
				renderList.Add(actor);
			}

			staticMeshOctree.QueryAABB(staticMeshActorQuery, aabb);

			for (auto &meshActor : staticMeshActorQuery) {
				renderList.Add(meshActor);
			}

			FrustumCulling::CullActorsFromScene(this, camera);

			FrustumCulling::CullLightsFromScene(this, camera);

		}

		void Scene::Clear() {

			sky = Lighting::Sky();
			postProcessing = PostProcessing::PostProcessing();

			terrains.clear();

			movableMeshActorOctree->Clear();
			staticMeshActorOctree->Clear();
			decalActorOctree->Clear();

			SceneNode::Clear();

		}

	}

}