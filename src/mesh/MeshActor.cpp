#include "MeshActor.h"

namespace Atlas {

	namespace Mesh {

		MeshActor::MeshActor(Mesh* mesh) : mesh(mesh) {

			modelMatrix = mat4(1.0f);

			castShadow = true;

			visible = true;

		}

		void MeshActor::Update() {



		}

	}

}