#include "Scene.h"
#include "FrustumCulling.h"

namespace Atlas {

	namespace Scene {

		Scene::Scene() : SceneNode() {

			renderList = RenderList(AE_OPAQUE_RENDERLIST);

			Common::AABB aabb(vec3(-2048.0f), vec3(2048.0f));
			meshOctree = Common::Octree<Actor::MeshActor*>(aabb, 8, 11);
			decalOctree = Common::Octree<Actor::DecalActor*>(aabb, 1, 11);

			AddToScene(&meshOctree, &decalOctree);

		}

		Scene::~Scene() {



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

		void Scene::Update(Camera *camera, float deltaTime) {

			renderList.Clear();

			for (auto &terrain : terrains) {
				terrain->Update(camera);
			}

			auto accumulatedLights = std::vector<Lighting::Light*>();

			SceneNode::Update(camera, deltaTime, accumulatedLights, mat4(1.0f), false);

			std::unordered_set<Actor::MeshActor*> meshQueryResult;

			for (auto &light : accumulatedLights) {
				// We always want to render all lights
				renderList.Add(light);
				// Check if the renderlist of the shadow needs an update
				if (light->GetShadow()) {
					if (!light->GetShadow()->update)
						continue;

					for (auto& component : light->GetShadow()->components) {
						component.renderList->Clear();
						meshQueryResult.clear();

						mat4 inverseMatrix = glm::inverse(component.projectionMatrix * component.viewMatrix);

						Common::AABB base(vec3(-1.0f), vec3(1.0f));

						auto aabb = base.Transform(inverseMatrix);

						meshOctree.QueryAABB(meshQueryResult, aabb);

						for (auto &meshActor : meshQueryResult) {
							component.renderList->Add(meshActor);
						}

					}
				}
			}

			meshQueryResult.clear();

			mat4 inverseMatrix = glm::inverse(camera->projectionMatrix * camera->viewMatrix);

			Common::AABB base(vec3(-1.0f), vec3(1.0f));

            auto aabb = base.Transform(glm::inverse(camera->projectionMatrix * camera->viewMatrix));

			meshOctree.QueryAABB(meshQueryResult, aabb);

			for (auto& actor : meshQueryResult) {
				renderList.Add(actor);
			}

			FrustumCulling::CullActorsFromScene(this, camera);

			FrustumCulling::CullLightsFromScene(this, camera);

		}

		void Scene::Clear() {

			sky = Lighting::Sky();
			postProcessing = PostProcessing::PostProcessing();

			terrains.clear();

			meshActorOctree->Clear();
			decalActorOctree->Clear();

			SceneNode::Clear();

		}

	}

}