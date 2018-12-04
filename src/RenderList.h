#ifndef RENDERLIST_H
#define RENDERLIST_H

#include "System.h"
#include "lighting/ILight.h"
#include "mesh/ActorBatch.h"

#include <map>

#define GEOMETRY_RENDERLIST 0
#define SHADOW_RENDERLIST  1

typedef struct RenderListBatch {

	ActorBatch* actorBatch;
	vector<MeshSubData*> subData;

}RenderListBatch;


class RenderList {

public:
	RenderList(int32_t type, int32_t mobility);

	void Add(Actor* actor);

	void Add(ILight* light);

	/**
	Can be called from the outside, like the scene might call it on their object
	*/
	void RemoveMesh(Mesh* mesh);

	void Clear();
	
	vector<ILight*> lights;
	map<Mesh*, ActorBatch*> actorBatches;
	map<int32_t, vector<RenderListBatch>> orderedRenderBatches;

private:
	int32_t type;
	int32_t mobility;

};


#endif