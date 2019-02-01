#ifndef RENDERLIST_H
#define RENDERLIST_H

#include "System.h"
#include "lighting/ILight.h"
#include "mesh/MeshActorBatch.h"

#include <map>

#define AE_GEOMETRY_RENDERLIST 0
#define AE_SHADOW_RENDERLIST  1

typedef struct RenderListBatch {

	MeshActorBatch* meshActorBatch;
	vector<MeshSubData*> subData;

}RenderListBatch;


class RenderList {

public:
	RenderList(int32_t type, int32_t mobility);

	void Add(MeshActor* actor);

	void Add(ILight* light);

	void RemoveMesh(Mesh* mesh);

	void Clear();
	
	vector<ILight*> lights;
	map<Mesh*, MeshActorBatch*> actorBatches;
	map<int32_t, vector<RenderListBatch>> orderedRenderBatches;

private:
	int32_t type;
	int32_t mobility;

};


#endif