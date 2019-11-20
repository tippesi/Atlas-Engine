#include "Scene.h"
#include "FrustumCulling.h"

namespace Atlas {

	namespace Scene {

		Scene::Scene(vec3 min, vec3 max) : SceneNode(), SpacePartitioning(min, max, 8) {

			AddToScene(this);

		}

		Scene::~Scene() {



		}

		Scene& Scene::operator=(const Scene& that) {

			if (this != &that) {

				SceneNode::operator=(that);

				terrains = that.terrains;
				sky = that.sky;
				postProcessing = that.postProcessing;

				hasChanged = true;

			}

			return *this;

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

			for (auto &terrain : terrains) {
				terrain->Update(camera);
			}

			if (ocean)
				ocean->Update(camera);
			
			hasChanged = SceneNode::Update(camera, deltaTime, mat4(1.0f), false);

		}

		bool Scene::HasChanged() {

			return hasChanged;

		}

		void Scene::Clear() {

			sky = Lighting::Sky();
			postProcessing = PostProcessing::PostProcessing();

			terrains.clear();

			SceneNode::Clear();
			SpacePartitioning::Clear();

		}

	}

}