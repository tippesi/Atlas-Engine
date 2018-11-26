#ifndef SCENENODE_H
#define SCENENODE_H

#include "System.h"
#include "mesh/Actor.h"
#include "lighting/Light.h"
#include <vector>

// Forward declaration of scene class
class Scene;

class SceneNode {

public:
	///
	SceneNode();

	///
	/// \param node
	void Add(SceneNode* node);

	///
	/// \param node
	void Remove(SceneNode* node);

	///
	/// \param actor
	void Add(Actor* actor);

	///
	/// \param actor
	void Remove(Actor* actor);

	///
	/// \param light
	void Add(Light* light);

	///
	/// \param light
	void Remove(Light* light);

	///
	/// \param parentTransformation
	void Update(mat4 parentTransformation);

	///
	/// \param scene
	void AddToScene(Scene* scene);

	///
	void RemoveFromScene();

	vector<SceneNode*> GetChildNodes();

	vector<Actor*> GetActors();

	vector<Light*> GetLights();

	Scene* scene;

	mat4 transformationMatrix;

private:
	bool sceneSet;

	vector<SceneNode*> childNodes;
	vector<Actor*> actors;
	vector<Light*> lights;

};

#endif