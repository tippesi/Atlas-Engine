#ifndef AE_SCENENODE_H
#define AE_SCENENODE_H

#include "System.h"
#include "mesh/MeshActor.h"
#include "lighting/Light.h"
#include <vector>

namespace Atlas {

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
		void Add(SceneNode *node);

		///
		/// \param node
		void Remove(SceneNode *node);

		///
		/// \param actor
		void Add(Mesh::MeshActor *actor);

		///
		/// \param actor
		void Remove(Mesh::MeshActor *actor);

		///
		/// \param light
		void Add(Lighting::Light *light);

		///
		/// \param light
		void Remove(Lighting::Light *light);

		///
		/// \param parentTransformation
		void Update(mat4 parentTransformation);

		///
		/// \param scene
		void AddToScene(Scene *scene);

		///
		void RemoveFromScene();

		std::vector<SceneNode *> GetChildNodes();

		std::vector<Mesh::MeshActor *> GetMeshActors();

		std::vector<Lighting::Light *> GetLights();

		Scene *scene;

		mat4 transformationMatrix;

	private:
		bool sceneSet;

		std::vector<SceneNode *> childNodes;
		std::vector<Mesh::MeshActor *> meshActors;
		std::vector<Lighting::Light *> lights;

	};

}

#endif