#ifndef AE_MODELLOADER_H
#define AE_MODELLOADER_H

#include "../System.h"
#include "../mesh/MeshData.h"
#include "../scene/SceneNode.h"

#include <Assimp/include/assimp/Importer.hpp>
#include <Assimp/include/assimp/scene.h>
#include <Assimp/include/assimp/postprocess.h>
#include <Assimp/include/assimp/types.h>

namespace Atlas {

	namespace Loader {

		class ModelLoader {

		public:
			static void LoadMesh(std::string filename, Mesh::MeshData& meshData);

		private:
			static void LoadMaterial(aiMaterial* assimpMaterial, Material& material, std::string directory);

		};

	}

}

#endif