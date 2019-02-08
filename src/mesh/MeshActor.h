#ifndef AE_MESHACTOR_H
#define AE_MESHACTOR_H

#include "../System.h"
#include "Mesh.h"

namespace Atlas {

	namespace Mesh {

		class MeshActor {

		public:

			/**
             *
             * @param mesh
             */
			MeshActor(Mesh* mesh);

			void Update();

			mat4 modelMatrix;
			mat4 transformedMatrix;

			Mesh* const mesh;

			bool castShadow;

			bool visible;

		};


	}

}

#endif