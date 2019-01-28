#ifndef MODELLOADER_H
#define MODELLOADER_H

#include "../System.h"
#include "../mesh/MeshData.h"
#include "../SceneNode.h"

#include <Assimp/include/assimp/Importer.hpp>
#include <Assimp/include/assimp/scene.h>
#include <Assimp/include/assimp/postprocess.h>
#include <Assimp/include/assimp/types.h>

class ModelLoader {

public:
	static MeshData* LoadMesh(string filename);

	static SceneNode* LoadSceneNode(string filename);

private:
	static Material* LoadMaterial(aiMaterial* assimpMaterial, string directory);

};


#endif
