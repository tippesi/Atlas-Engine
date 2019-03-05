#include "StaticMeshActor.h"

namespace Atlas {

	namespace Actor {

		StaticMeshActor::StaticMeshActor(Mesh::Mesh* mesh, mat4 matrix) : mesh(mesh) {

			Actor::SetMatrix(matrix);

		}

		void StaticMeshActor::Update(float deltaTime, mat4 parentTransform, bool parentUpdate) {



		}

	}

}