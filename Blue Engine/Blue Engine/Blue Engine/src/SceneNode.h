#ifndef SCENENODE_H
#define SCENENODE_H

#include "system.h"
#include "mesh/actor.h"
#include "light.h"
#include <vector>

// Forward declaration of scene
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

	Scene* scene;

	vector<SceneNode*> nodes;
	vector<Actor*> actors;
	vector<Light*> lights;

};

#endif