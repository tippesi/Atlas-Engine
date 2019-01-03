#ifndef MODELLOADER_H
#define MODELLOADER_H

#include "../System.h"
#include "../mesh/MeshData.h"
#include "../SceneNode.h"

#include "../libraries/assimp/Importer.hpp"
#include "../libraries/assimp/scene.h"
#include "../libraries/assimp/postprocess.h"
#include "../libraries/assimp/types.h"
#include "../libraries/assimp/config.h"

class ModelLoader {

public:
	static MeshData* LoadMesh(string filename);

	static SceneNode* LoadSceneNode(string filename);

private:
	static Material* LoadMaterial(aiMaterial* assimpMaterial, string directory);

};


#endif
