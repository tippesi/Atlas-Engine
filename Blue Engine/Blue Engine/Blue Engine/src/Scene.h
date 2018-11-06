#ifndef SCENE_H
#define SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "Mesh/Actor.h"
#include "Mesh/ActorBatch.h"
#include "Lighting/Light.h"
#include "PostProcessing/PostProcessing.h"

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

	PostProcessing* postProcessing;

};

#endif