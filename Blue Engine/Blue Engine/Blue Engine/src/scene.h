#ifndef SCENE_H
#define SCENE_H

#include "system.h"
#include "scenenode.h"
#include "mesh/actor.h"
#include "mesh/actorbatch.h"
#include "light.h"

class Scene {

public:
	Scene();

	void Add(SceneNode* node);

	void Remove(SceneNode* node);

	void Add(Actor* actor);

	void Remove(Actor* actor);

	void Add(Light* light);

	void Remove(Light* light);

	void Update();

	SceneNode* rootNode;

	vector<Light*> lights;
	vector<ActorBatch*> actorBatches;

};

#endif