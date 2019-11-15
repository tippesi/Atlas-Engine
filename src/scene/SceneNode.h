#ifndef AE_SCENENODE_H
#define AE_SCENENODE_H

#include "../System.h"
#include "../actor/MovableMeshActor.h"
#include "../actor/StaticMeshActor.h"
#include "../actor/DecalActor.h"
#include "../lighting/Light.h"
#include "../Decal.h"

#include "SpacePartitioning.h"

#include <vector>

namespace Atlas {

	namespace Scene {

		class SceneNode {

		public:
			/**
             *
             */
			SceneNode() {}

			SceneNode(const SceneNode& that);

			/**
			 * 
			 */
			SceneNode(SpacePartitioning* partitioning) { AddToScene(partitioning); }

			/**
			 * 
			 */
			virtual ~SceneNode() {}


			SceneNode& operator=(const SceneNode& that);

			/**
			 *
			 * @param node
			 * @note A scene node can only be attached to one node.
			 */
			virtual void Add(SceneNode *node);

			/**
			 *
			 * @param node
			 */
			virtual void Remove(SceneNode *node);

			/**
			 *
			 * @param actor
			 * @note An actor can only be attached to one node.
			 */
			virtual void Add(Actor::MovableMeshActor *actor);

			/**
			 *
			 * @param actor
			 */
			virtual void Remove(Actor::MovableMeshActor *actor);

			/**
			 *
			 * @param actor
			 * @note An actor can only be attached to one node.
			 */
			virtual void Add(Actor::StaticMeshActor *actor);

			/**
			 *
			 * @param actor
			 */
			virtual void Remove(Actor::StaticMeshActor *actor);

			/**
			 *
			 * @param decal
			 * @note An actor can only be attached to one node.
			 */
			virtual void Add(Actor::DecalActor* decal);

			/**
			 *
			 * @param decal
			 */
			virtual void Remove(Actor::DecalActor* decal);

			/**
			 *
			 * @param decal
			 * @note An actor can only be attached to one node.
			 */
			virtual void Add(Actor::AudioActor* decal);

			/**
			 *
			 * @param decal
			 */
			virtual void Remove(Actor::AudioActor* decal);

			/**
			 *
			 * @param light
			 */
			virtual void Add(Lighting::Light *light);

			/**
			 *
			 * @param light
			 */
			virtual void Remove(Lighting::Light *light);

			/**
			 *
			 * @param matrix
			 */
			virtual void SetMatrix(mat4 matrix);


			virtual void Clear();

		protected:
			/**
			 *
			 * @param scene
			 */
			virtual void AddToScene(SpacePartitioning* spacePartitioning);

			/**
			 *
			 */
			virtual void RemoveFromScene();

			/**
			 *
			 * @param deltaTime
			 * @param parentTransformation
			 * @param parentTransformChanged
			 * @return True if something has changed, false otherwise
			 */
			virtual bool Update(Camera* camera, float deltaTime, mat4 parentTransformation,
				bool parentTransformChanged);

			void DeepCopy(const SceneNode& that);

			bool sceneSet = false;
			bool matrixChanged = true;

			mat4 matrix;
			mat4 transformedMatrix;

			std::vector<SceneNode*> childNodes;
			std::vector<Actor::MovableMeshActor*> movableMeshActors;
			std::vector<Actor::StaticMeshActor*> staticMeshActors;
			std::vector<Actor::DecalActor*> decalActors;
			std::vector<Actor::AudioActor*> audioActors;
			std::vector<Lighting::Light*> lights;

			std::vector<Actor::StaticMeshActor*> addableStaticMeshActors;

			SpacePartitioning* spacePartitioning = nullptr;

		};

	}

}

#endif