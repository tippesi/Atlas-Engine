#ifndef AE_SCENE_H
#define AE_SCENE_H

#include "../System.h"
#include "../actor/MeshActor.h"
#include "../terrain/Terrain.h"
#include "../lighting/Light.h"
#include "../lighting/Sky.h"
#include "../postprocessing/PostProcessing.h"
#include "../Decal.h"

#include "SceneNode.h"
#include "SpacePartitioning.h"

namespace Atlas {

	namespace Scene {

		class Scene : public SceneNode, public SpacePartitioning {

		public:

			/**
             * Constructs a scene object.
			 * @param min
			 * @param max
             */
			Scene(vec3 min, vec3 max);

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
			 * @param deltaTime
             * @remark The update call does the following:
             * - First traverse the scene tree to retrieve information about all actors and update them
             * - Do the frustum culling on the actors returned, order them if they need updates for their current settings
             * - Gather all light data and fill the renderlists with the actors visible for these lights
             */
			void Update(Camera *camera, float deltaTime);

			/**
			 * Renamed from SceneNode: A scene shouldn't be transformed.
			 */
			void SetMatrix() {}

			/**
			 * Removes everthing from the scene.
			 */
			void Clear();

			/**
			 * To overload the Add and Remove methods we need to specify this
			 * here. It would just rename the method instead.
			 */
			using SceneNode::Add;
			using SceneNode::Remove;

			std::vector<Terrain::Terrain *> terrains;

			Lighting::Sky sky;
			PostProcessing::PostProcessing postProcessing;

		};

	}

}

#endif