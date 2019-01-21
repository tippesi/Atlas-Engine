#ifndef SCENE_H
#define SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "RenderList.h"
#include "mesh/MeshActor.h"
#include "mesh/MeshActorBatch.h"
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

	~Scene();

	/**
	 *
	 * @param actor
	 */
	void Add(MeshActor* actor);

	/**
	 *
	 * @param actor
	 */
	void Remove(MeshActor* actor);

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

	SceneNode* rootNode;

	vector<ILight*> lights;
	vector<MeshActorBatch*> meshActorBatches;
	vector<Terrain*> terrains;
	vector<Decal*> decals;

	Sky* sky;
	PostProcessing* postProcessing;

	RenderList* renderList;

};

#endif