#include "ResourceManager.h"
#include "common/Path.h"

#include "loader/AssetLoader.h"
#include "loader/MaterialLoader.h"
#include "loader/ModelLoader.h"
#include "loader/TerrainLoader.h"

#include <algorithm>

namespace Atlas {

    std::vector<Mesh::Mesh*> ResourceManager::meshes;
    std::vector<Material*> ResourceManager::materials;
    std::vector<Audio::AudioData*> ResourceManager::audios;
    std::vector<Terrain::Terrain*> ResourceManager::terrains;
    std::vector<Scene::Scene*> ResourceManager::scenes;

    std::mutex ResourceManager::meshMutex;
    std::mutex ResourceManager::materialMutex;
    std::mutex ResourceManager::audioMutex;
    std::mutex ResourceManager::terrainMutex;
    std::mutex ResourceManager::sceneMutex;

    Mesh::Mesh* ResourceManager::GetMesh(std::string path, bool forceTangents) {

        path = Loader::AssetLoader::GetFullPath(path);
        path = Common::Path::GetAbsolute(path);

        std::lock_guard<std::mutex> guard(meshMutex);

        for (auto mesh : meshes) {
            if (mesh->data.filename == path)
                return mesh;
        }

        auto meshData = Loader::ModelLoader::LoadMesh(path, forceTangents);
        auto mesh = new Atlas::Mesh::Mesh(meshData);
        meshes.push_back(mesh);

        std::lock_guard<std::mutex> guardMaterial(materialMutex);

        for (auto& material : mesh->data.materials) {
            materials.push_back(&material);
        }

        return mesh;

    }

    Material* ResourceManager::GetMaterial(std::string path) {

        /*
        path = Loader::AssetLoader::GetFullPath(path);
        path = Common::Path::GetAbsolute(path);

        std::lock_guard<std::mutex> guard(materialMutex);

        for (auto mat : materials) {
            if (mat->name == path)
                return mat;
        }

        auto material = Atlas::Loader::MaterialLoader::LoadMaterial(path);

        if (material)
            materials.push_back(material);

        return material;
        */
        return nullptr;

    }

    Audio::AudioData* ResourceManager::GetAudio(std::string path) {

        path = Loader::AssetLoader::GetFullPath(path);
        path = Common::Path::GetAbsolute(path);

        std::lock_guard<std::mutex> guard(audioMutex);

        for (auto audio : audios) {
            if (audio->filename == path)
                return audio;
        }

        auto audio = new Atlas::Audio::AudioData(path);
        audios.push_back(audio);

        return audio;

    }

    Terrain::Terrain* ResourceManager::GetTerrain(std::string path) {

        path = Loader::AssetLoader::GetFullPath(path);
        path = Common::Path::GetAbsolute(path);

        std::lock_guard<std::mutex> guard(terrainMutex);

        for (auto terrain : terrains) {
            if (terrain->filename == path)
                return terrain;
        }

        auto terrain = Atlas::Loader::TerrainLoader::LoadTerrain(path);

        /*
        if (terrain) {
            terrains.push_back(terrain);

            std::lock_guard<std::mutex> guardMaterial(materialMutex);

            auto mats = terrain->storage->GetMaterials();
            for (auto mat : mats) {
                if (!mat)
                    continue;
                materials.push_back(mat);
            }
        }
        */

        return nullptr;

    }

    void ResourceManager::AddMaterial(Material* material) {

        std::lock_guard<std::mutex> guard(materialMutex);
        materials.push_back(material);

    }

    void ResourceManager::AddTerrain(Terrain::Terrain* terrain) {

        std::lock_guard<std::mutex> guard(terrainMutex);
        terrains.push_back(terrain);

    }

    void ResourceManager::AddScene(Scene::Scene* scene) {

        std::lock_guard<std::mutex> guard(sceneMutex);
        scenes.push_back(scene);

    }

    void ResourceManager::RemoveMaterial(Material* material) {

        std::lock_guard<std::mutex> guard(materialMutex);

        auto it = std::find(materials.begin(), materials.end(), material);

        if (it != materials.end()) {
            materials.erase(it);
        }

    }

    void ResourceManager::RemoveTerrain(Terrain::Terrain* terrain) {

        std::lock_guard<std::mutex> guard(terrainMutex);

        auto it = std::find(terrains.begin(), terrains.end(), terrain);

        if (it != terrains.end()) {
            terrains.erase(it);
        }

    }

    std::vector<Mesh::Mesh*> ResourceManager::GetMeshes() {

        return meshes;

    }

    std::vector<Material*> ResourceManager::GetMaterials() {

        return materials;

    }

    std::vector<Audio::AudioData*> ResourceManager::GetAudios() {

        return audios;

    }

    std::vector<Terrain::Terrain*> ResourceManager::GetTerrains() {

        return terrains;

    }

    std::vector<Scene::Scene*> ResourceManager::GetScenes() {

        return scenes;

    }

}