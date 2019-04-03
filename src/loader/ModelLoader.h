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
			static Mesh::MeshData* LoadMesh(std::string filename);

			static Scene::SceneNode* LoadSceneNode(std::string filename);

		private:
			static Material* LoadMaterial(aiMaterial* assimpMaterial, std::string directory);

		};

	}

}

#endif