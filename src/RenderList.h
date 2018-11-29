#ifndef RENDERLIST_H
#define RENDERLIST_H

#include "System.h"
#include "lighting/Light.h"
#include "mesh/ActorBatch.h"

class RenderList {

public:
	RenderList();

	void Add(Actor* actor);

	void Add(Light* light);

	void Clear();

	vector<ActorBatch*> actorBatches;
	vector<Light*> lights;


};


#endif