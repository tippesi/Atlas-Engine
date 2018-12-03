#ifndef RENDERLIST_H
#define RENDERLIST_H

#include "System.h"
#include "lighting/Light.h"
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
	RenderList(int32_t type);

	void Add(Actor* actor);

	void Add(Light* light);

	void Clear();
	
	vector<Light*> lights;
	map<Mesh*, ActorBatch*> actorBatches;
	map<int32_t, vector<RenderListBatch>> orderedRenderBatches;

private:
	int32_t type;

};


#endif