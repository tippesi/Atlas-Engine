#ifndef AE_MESHACTOR_H
#define AE_MESHACTOR_H

#include "System.h"
#include "Actor.h"
#include "../mesh/Mesh.h"

namespace Atlas {

	namespace Actor {

		class MeshActor : public Actor {

		public:
			/**
             *
             * @param mesh
             */
			MeshActor(Mesh::Mesh* mesh);

			void Update(float deltaTime, mat4 parentTransform, bool parentUpdate);

			Mesh::Mesh* const mesh;

			bool castShadow;

			bool visible;

		};


	}

}

#endif