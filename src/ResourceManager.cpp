#include "ResourceManager.h"
#include "common/Path.h"
#include "loader/AssetLoader.h"

namespace Atlas {

	std::vector<Mesh::Mesh*> ResourceManager::meshes;

	std::vector<std::string> ResourceManager::loadingFiles;

	std::mutex ResourceManager::meshMutex;

	Mesh::Mesh* ResourceManager::GetMesh(std::string path) {

		path = Common::Path::GetAbsolute(path);

		std::lock_guard<std::mutex> guard(meshMutex);

		for (auto mesh : meshes) {
			if (mesh->data.filename == path)
				return mesh;
		}

		auto mesh = new Atlas::Mesh::Mesh(path);
		meshes.push_back(mesh);

		return mesh;

	}

	std::vector<Mesh::Mesh*> ResourceManager::GetMeshes() {

		return meshes;

	}

}