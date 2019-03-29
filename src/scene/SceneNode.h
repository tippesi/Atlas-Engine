#ifndef AE_SCENENODE_H
#define AE_SCENENODE_H

#include "../System.h"
#include "../actor/MovableMeshActor.h"
#include "../actor/StaticMeshActor.h"
#include "../actor/DecalActor.h"
#include "../lighting/Light.h"
#include "../common/Octree.h"
#include "../Decal.h"
#include <vector>

namespace Atlas {

	namespace Scene {

		class SceneNode {

		public:
			/**
             *
             */
			SceneNode();

			virtual ~SceneNode() {};

			/**
			 *
			 * @param node
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
			 */
			virtual void Add(Actor::DecalActor* decal);

			/**
			 *
			 * @param decal
			 */
			virtual void Remove(Actor::DecalActor* decal);

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
			virtual void AddToScene(Common::Octree<Actor::MovableMeshActor*>* movableMeshActorOctree,
				Common::Octree<Actor::StaticMeshActor*>* staticMeshActorOctree,
				Common::Octree<Actor::DecalActor*>* decalActorOctree);

			/**
			 *
			 */
			virtual void RemoveFromScene();

			/**
			 *
			 * @param deltaTime
			 * @param parentTransformation
			 * @param parentTransformChanged
			 */
			virtual void Update(Camera* camera, float deltaTime, std::vector<Lighting::Light*>& lights,
					mat4 parentTransformation, bool parentTransformChanged);

			bool sceneSet;
			bool matrixChanged;

			mat4 matrix;
			mat4 transformedMatrix;

			std::vector<SceneNode*> childNodes;
			std::vector<Actor::MovableMeshActor*> movableMeshActors;
			std::vector<Actor::StaticMeshActor*> staticMeshActors;
			std::vector<Actor::DecalActor*> decalActors;
			std::vector<Lighting::Light *> lights;

			Common::Octree<Actor::MovableMeshActor*>* movableMeshActorOctree;
			Common::Octree<Actor::StaticMeshActor*>* staticMeshActorOctree;
			Common::Octree<Actor::DecalActor*>* decalActorOctree;

		};

	}

}

#endif