#ifndef MODELLOADER_H
#define MODELLOADER_H

#include "../system.h"
#include "../mesh/meshdata.h"

class ModelLoader {

public:
	static MeshData* LoadMesh(const char* filename);

};


#endif