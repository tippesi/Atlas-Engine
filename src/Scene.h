#ifndef AE_SCENE_H
#define AE_SCENE_H

#include "System.h"
#include "SceneNode.h"
#include "RenderList.h"
#include "mesh/MeshActor.h"
#include "mesh/MeshActorBatch.h"
#include "terrain/Terrain.h"
#include "lighting/ILight.h"
#include "lighting/Sky.h"
#include "postprocessing/PostProcessing.h"
#include "Decal.h"

namespace Atlas {

	class Scene {

	public:

		/**
         *
         */
		Scene();

		~Scene();

		/**
         *
         * @param actor
         */
		void Add(Mesh::MeshActor *actor);

		/**
         *
         * @param actor
         */
		void Remove(Mesh::MeshActor *actor);

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
         * @param light
         */
		void Add(Lighting::ILight *light);

		/**
         *
         * @param light
         */
		void Remove(Lighting::ILight *light);

		/**
         *
         * @param decal
         */
		void Add(Decal *decal);

		/**
         *
         * @param decal
         */
		void Remove(Decal *decal);

		/**
         *
         * @param camera
         */
		void Update(Camera *camera);

		/**
         *
         */
		void ClearContent();

		/**
         *
         */
		void DeleteContent();

		SceneNode *rootNode;

		std::vector<Lighting::ILight *> lights;
		std::vector<Mesh::MeshActorBatch *> meshActorBatches;
		std::vector<Terrain::Terrain *> terrains;
		std::vector<Decal *> decals;

		Lighting::Sky *sky;
		PostProcessing::PostProcessing *postProcessing;

		RenderList *renderList;

	};

}

#endif