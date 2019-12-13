#ifndef AE_SCENE_H
#define AE_SCENE_H

#include "../System.h"
#include "../actor/MeshActor.h"
#include "../terrain/Terrain.h"
#include "../lighting/Light.h"
#include "../lighting/Sky.h"
#include "../ocean/Ocean.h"
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
			 */
			Scene() : SceneNode(this), SpacePartitioning(vec3(-2048.0f), vec3(2048.0f), 5) {}

			/**
			 * Constructs a scene object.
			 * @param min The minimum scene boundary.
			 * @param max The maximum scene boundary.
			 * @param depth The maximum depths of the octrees.
			 * @note The boundary should be as tightly fitted to the scene as possible.
			 * It determines the size of the octrees.
			 */
			Scene(vec3 min, vec3 max, int32_t depth = 5);

			/**
			 * Destructs a scene object.
			 */
			~Scene();


			Scene& operator=(const Scene& that);

			/**
             *
             * @param camera
			 * @param deltaTime
             * @remark 
             */
			void Update(Camera *camera, float deltaTime);

			/**
			 * Checks whether the scene has changed since the last update
			 * @return True if scene has changed, false otherwise.
			 */
			bool HasChanged();

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

			Terrain::Terrain* terrain = nullptr;
			Ocean::Ocean* ocean = nullptr;

			Lighting::Sky sky;
			PostProcessing::PostProcessing postProcessing;

		private:
			bool hasChanged = true;

		};

	}

}

#endif