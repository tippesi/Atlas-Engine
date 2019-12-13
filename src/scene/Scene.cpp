#include "Scene.h"

namespace Atlas {

	namespace Scene {

		Scene::Scene(vec3 min, vec3 max, int32_t depth) : SceneNode(),
			SpacePartitioning(min, max, depth) {

			AddToScene(this);

		}

		Scene::~Scene() {



		}

		Scene& Scene::operator=(const Scene& that) {

			if (this != &that) {

				SceneNode::operator=(that);
				SpacePartitioning::operator=(that);

				terrain = that.terrain;
				ocean = that.ocean;
				sky = that.sky;
				postProcessing = that.postProcessing;

				hasChanged = true;

			}

			return *this;

		}

		void Scene::Update(Camera *camera, float deltaTime) {

			if (terrain) {
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

			SceneNode::Clear();
			SpacePartitioning::Clear();

		}

	}

}