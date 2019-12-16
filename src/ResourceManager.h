#ifndef AE_RESOURCEMANAGER_H
#define AE_RESOURCEMANAGER_H

#include "System.h"

#include "mesh/Mesh.h"

#include <mutex>

namespace Atlas {

	class ResourceManager {

	public:
		static Mesh::Mesh* GetMesh(std::string path);

		static std::vector<Mesh::Mesh*> GetMeshes();

	private:
		bool CheckLoadingFiles(std::string path);

		static std::vector<Mesh::Mesh*> meshes;

		static std::vector<std::string> loadingFiles;

		static std::mutex meshMutex;

	};

}

#endif