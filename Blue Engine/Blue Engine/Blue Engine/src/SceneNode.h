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
	SceneNode();

	void Add(SceneNode* node);

	void Remove(SceneNode* node);

	void Add(Actor* actor);

	void Remove(Actor* actor);

	void Add(Light* light);

	void Remove(Light* light);

	void Update(mat4 parentTransformation);

	void AddToScene(Scene* scene);

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