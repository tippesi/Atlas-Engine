#include "Scene.h"

namespace Atlas {

    namespace Scene {

        Scene::Scene(vec3 min, vec3 max, int32_t depth) : SceneNode(),
            SpacePartitioning(min, max, depth), rtData(this) {

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

            auto meshes = GetMeshes();

            if (terrain) {
                terrain->Update(camera);
            }

            if (ocean)
                ocean->Update(camera, deltaTime);

            if (sky.sun) {
                sky.sun->Update(camera);
            }

            hasChanged = SceneNode::Update(camera, deltaTime, mat4(1.0f), false);

            // Make sure this is changed just once at the start of a frame
            rtDataValid = rtData.IsValid();
            if (rtDataValid) {
                rtData.Update(true);
            }

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

        std::vector<ResourceHandle<Mesh::Mesh>> Scene::GetMeshes() {

            std::vector<ResourceHandle<Mesh::Mesh>> meshes;

            // Not really efficient, but does the job
            for (auto& [meshId, registeredMesh] : rootMeshMap) {
                meshes.push_back(registeredMesh.mesh);
            }

            return meshes;

        }

        std::vector<Material*> Scene::GetMaterials() {

            std::vector<Material*> materials;

            if (terrain) {
                auto terrainMaterials = terrain->storage.GetMaterials();

                for (auto material : terrainMaterials) {
                    if (!material)
                        continue;

                    materials.push_back(material.get());
                }
                
            }

            auto meshes = GetMeshes();
            if (vegetation) {
                auto vegMeshes = vegetation->GetMeshes();
                meshes.insert(meshes.end(), vegMeshes.begin(), vegMeshes.end());
            }

            for (auto mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;
                for (auto& material : mesh->data.materials) {
                    materials.push_back(material.get());
                }
            }

            return materials;

        }

        void Scene::BuildRTStructures() {

            rtData.Build();

        }

        void Scene::ClearRTStructures() {

            rtData.Clear();

        }

        void Scene::WaitForResourceLoad() {

            auto meshes = GetMeshes();

            for(auto mesh : meshes) {
                mesh.WaitForLoad();
            }

        }

        bool Scene::IsFullyLoaded() {

            bool loaded = true;

            auto meshes = GetMeshes();

            for(auto mesh : meshes) {
                loaded &= mesh.IsLoaded();
            }

            return loaded;

        }

        bool Scene::IsRtDataValid() {

            return rtDataValid;

        }

    }

}