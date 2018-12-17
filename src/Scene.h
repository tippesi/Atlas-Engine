#ifndef SCENE_H
#define SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "RenderList.h"
#include "mesh/Actor.h"
#include "mesh/ActorBatch.h"
#include "terrain/Terrain.h"
#include "lighting/ILight.h"
#include "lighting/Sky.h"
#include "postprocessing/PostProcessing.h"
#include "Decal.h"

class Scene {

public:

	/**
	 *
	 */
	Scene();

	/**
	 *
	 * @param actor
	 */
	void Add(Actor* actor);

	/**
	 *
	 * @param actor
	 */
	void Remove(Actor* actor);

	/**
	 *
	 * @param terrain
	 */
	void Add(Terrain* terrain);

	/**
	 *
	 * @param terrain
	 */
	void Remove(Terrain* terrain);

	/**
	 *
	 * @param light
	 */
	void Add(ILight* light);

	/**
	 *
	 * @param light
	 */
	void Remove(ILight* light);

	/**
	 *
	 * @param decal
	 */
	void Add(Decal* decal);

	/**
	 *
	 * @param decal
	 */
	void Remove(Decal* decal);

	/**
	 *
	 * @param camera
	 */
	void Update(Camera* camera);

	/**
	 *
	 */
	void ClearContent();

	/**
	 *
	 */
	void DeleteContent();

	~Scene();

	SceneNode* rootNode;

	vector<ILight*> lights;
	vector<ActorBatch*> actorBatches;
	vector<Terrain*> terrains;
	vector<Decal*> decals;

	Sky* sky;
	PostProcessing* postProcessing;

	RenderList* renderList;

};

#endif