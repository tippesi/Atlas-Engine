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
			Scene() : SceneNode(this), SpacePartitioning(vec3(-2048.0f), vec3(2048.0f), 8) {}

			/**
             * Constructs a scene object.
			 * @param min
			 * @param max
             */
			Scene(vec3 min, vec3 max);

			/**
			 * Destructs a scene object.
			 */
			~Scene();


			Scene& operator=(const Scene& that);

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

			std::vector<Terrain::Terrain*> terrains;

			Lighting::Sky sky;
			PostProcessing::PostProcessing postProcessing;

			Ocean::Ocean* ocean = nullptr;

		private:
			bool hasChanged = true;

		};

	}

}

#endif