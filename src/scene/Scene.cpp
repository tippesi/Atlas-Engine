#include "Scene.h"

namespace Atlas {

	namespace Scene {

		Scene::Scene(vec3 min, vec3 max, int32_t depth) : SceneNode(),
			SpacePartitioning(min, max, depth), rayTracingData(this) {

			AddToScene(this, &rootMeshMap);

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
				ocean->Update(camera, deltaTime);
			
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

		std::vector<Mesh::Mesh*> Scene::GetMeshes() {

			std::vector<Mesh::Mesh*> meshes;

			// Not really efficient, but does the job
			for (auto& [mesh, count] : rootMeshMap) {
				meshes.push_back(mesh);
			}

			return meshes;

		}

		std::vector<Material*> Scene::GetMaterials() {

			std::vector<Material*> materials;

			if (terrain) {
				auto terrainMaterials = terrain->storage->GetMaterials();

				for (auto material : terrainMaterials) {
					if (!material)
						continue;

					materials.push_back(material);
				}
				
			}

			auto meshes = GetMeshes();
			if (vegetation) {
				auto vegMeshes = vegetation->GetMeshes();
				meshes.insert(meshes.end(), vegMeshes.begin(), vegMeshes.end());
			}

			for (auto mesh : meshes) {
				for (auto& material : mesh->data.materials) {
					materials.push_back(&material);
				}
			}

			return materials;

		}

		void Scene::BuildRTStructures() {

			rayTracingData.Update();

		}
	}

}