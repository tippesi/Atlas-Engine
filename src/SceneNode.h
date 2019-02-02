#ifndef SCENENODE_H
#define SCENENODE_H

#include "System.h"
#include "mesh/MeshActor.h"
#include "lighting/ILight.h"
#include <vector>

// Forward declaration of scene class
class Scene;

class SceneNode {

public:
	/**
	 *
	 */
	SceneNode();

	///
	/// \param node
	void Add(SceneNode* node);

	///
	/// \param node
	void Remove(SceneNode* node);

	///
	/// \param actor
	void Add(MeshActor* actor);

	///
	/// \param actor
	void Remove(MeshActor* actor);

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

	std::vector<SceneNode*> GetChildNodes();

	std::vector<MeshActor*> GetMeshActors();

	std::vector<ILight*> GetLights();

	Scene* scene;

	mat4 transformationMatrix;

private:
	bool sceneSet;

	std::vector<SceneNode*> childNodes;
	std::vector<MeshActor*> meshActors;
	std::vector<ILight*> lights;

};

#endif