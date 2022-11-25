#include "StaticMeshActor.h"

namespace Atlas {

	namespace Actor {

		StaticMeshActor::StaticMeshActor(Mesh::Mesh* mesh, mat4 matrix) : MeshActor(mesh) {

			Actor::SetMatrix(matrix);

		}

	}

}