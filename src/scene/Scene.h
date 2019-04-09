#ifndef AE_SCENE_H
#define AE_SCENE_H

#include "../System.h"
#include "SceneNode.h"
#include "../common/Octree.h"
#include "../actor/ActorBatch.h"
#include "../RenderList.h"
#include "../actor/MeshActor.h"
#include "../terrain/Terrain.h"
#include "../lighting/Light.h"
#include "../lighting/Sky.h"
#include "../postprocessing/PostProcessing.h"
#include "../Decal.h"

namespace Atlas {

	namespace Scene {

		class Scene : public SceneNode {

		public:

			/**
             * Constructs a scene object.
             */
			Scene();

			~Scene();

			/**
             *
             * @param terrain
             */
			void Add(Terrain::Terrain *terrain);

			/**
             *
             * @param terrain
             */
			void Remove(Terrain::Terrain *terrain);

			/**
             *
             * @param camera
             * @remark The update call does the following:
             * - First traverse the scene tree to retrieve information about all actors and update them
             * - Do the frustum culling on the actors returned, order them if they need updates for their current settings
             * - Gather all light data and fill the renderlists with the actors visible for these lights
             */
			void Update(Camera *camera, float deltaTime);

			void Clear();

			std::vector<Terrain::Terrain *> terrains;

			Lighting::Sky sky;
			PostProcessing::PostProcessing postProcessing;

			RenderList renderList;

			/**
			 * To overload the Add and Remove methods we need to specify this
			 * here. It would just rename the method instead.
			 */
			using SceneNode::Add;
			using SceneNode::Remove;

		private:
			Common::Octree<Actor::MovableMeshActor*> movableMeshOctree;
			Common::Octree<Actor::StaticMeshActor*> staticMeshOctree;
			Common::Octree<Actor::DecalActor*> decalOctree;

		};

	}

}

#endif