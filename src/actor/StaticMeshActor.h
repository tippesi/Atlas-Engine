#ifndef AE_STATICMESHACTOR_H
#define AE_STATICMESHACTOR_H

#include "MeshActor.h"

namespace Atlas {

	namespace Actor {

		class StaticMeshActor : public MeshActor {

		public:
			StaticMeshActor() : MeshActor(nullptr) {}

			StaticMeshActor(Mesh::Mesh* mesh, mat4 matrix);

			void SetMatrix() {}

		};

	}

}

#endif