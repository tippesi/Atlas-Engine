#ifndef SCENE_H
#define SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "RenderList.h"
#include "mesh/Actor.h"
#include "mesh/ActorBatch.h"
#include "terrain/Terrain.h"
#include "lighting/Light.h"
#include "lighting/Sky.h"
#include "postprocessing/PostProcessing.h"

class Scene {

public:
	///
	Scene();

	///
	/// \param actor
	void Add(Actor* actor);

	///
	/// \param actor
	void Remove(Actor* actor);

	///
	/// \param terrain
	void Add(Terrain* terrain);

	///
	/// \param terrain
	void Remove(Terrain* terrain);

	///
	/// \param light
	void Add(Light* light);

	///
	/// \param light
	void Remove(Light* light);

	///
	void Update();

	///
	void ClearContent();

	///
	void DeleteContent();

	~Scene();

	SceneNode* rootNode;

	vector<Light*> lights;
	vector<ActorBatch*> actorBatches;
	vector<Terrain*> terrains;

	Sky* sky;
	PostProcessing* postProcessing;

	RenderList* renderList;

};

#endif