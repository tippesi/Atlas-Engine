#ifndef SCENENODE_H
#define SCENENODE_H

#include "System.h"
#include "mesh/Actor.h"
#include "lighting/ILight.h"
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
	void Add(ILight* light);

	///
	/// \param light
	void Remove(ILight* light);

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

	vector<ILight*> GetLights();

	Scene* scene;

	mat4 transformationMatrix;

private:
	bool sceneSet;

	vector<SceneNode*> childNodes;
	vector<Actor*> actors;
	vector<ILight*> lights;

};

#endif