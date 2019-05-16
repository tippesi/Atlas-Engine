#include "MeshActor.h"

namespace Atlas {

	namespace Actor {

		void MeshActor::Update(Camera camera, float deltaTime,
			mat4 parentTransform, bool parentUpdate) {

			if (matrixChanged || parentUpdate) {

				matrixChanged = false;

				transformedMatrix = parentTransform * GetMatrix();

				aabb = mesh->data.aabb.Transform(transformedMatrix);

			}

		}

	}

}