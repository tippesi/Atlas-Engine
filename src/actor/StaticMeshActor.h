#ifndef AE_STATICMESHACTOR_H
#define AE_STATICMESHACTOR_H

#include "Actor.h"
#include "../mesh/Mesh.h"

namespace Atlas {

	namespace Actor {

		class StaticMeshActor : public Actor {

		public:
			StaticMeshActor(Mesh::Mesh* mesh, mat4 matrix);

			void Update(float deltaTime, mat4 parentTransform, bool parentUpdate);

			void SetMatrix() {}

			Mesh::Mesh* const mesh;

		};

	}

}

#endif