#ifndef SCENE_H
#define SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "mesh/Actor.h"
#include "mesh/ActorBatch.h"
#include "terrain/Terrain.h"
#include "lighting/Light.h"
#include "lighting/Sky.h"
#include "postprocessing/PostProcessing.h"

class Scene {

public:
	Scene();

	void Add(Actor* actor);

	void Remove(Actor* actor);

	void Add(Terrain* terrain);

	void Remove(Terrain* terrain);

	void Add(Light* light);

	void Remove(Light* light);

	void Update();

	void ClearContent();

	void DeleteContent();

	~Scene();

	SceneNode* rootNode;

	vector<Light*> lights;
	vector<ActorBatch*> actorBatches;
	vector<Terrain*> terrains;

	Sky* sky;
	PostProcessing* postProcessing;

};

#endif