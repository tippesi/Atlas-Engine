#ifndef SCENE_H
#define SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "mesh/Actor.h"
#include "mesh/ActorBatch.h"
#include "lighting/Light.h"
#include "lighting/Sky.h"
#include "postprocessing/PostProcessing.h"

class Scene {

public:
	Scene();

	void Add(Actor* actor);

	void Remove(Actor* actor);

	void Add(Light* light);

	void Remove(Light* light);

	void Update();

	~Scene();

	SceneNode* rootNode;

	vector<Light*> lights;
	vector<ActorBatch*> actorBatches;

	Sky* sky;
	PostProcessing* postProcessing;

};

#endif