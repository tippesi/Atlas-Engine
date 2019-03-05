#include "MeshActor.h"

namespace Atlas {

	namespace Actor {

		MeshActor::MeshActor(Mesh::Mesh* mesh) : mesh(mesh) {

			castShadow = true;

			visible = true;

		}

		void MeshActor::Update(float deltaTime, mat4 parentTransform, bool parentUpdate) {

			if (matrixChanged || parentUpdate) {

				matrixChanged = false;

				transformedMatrix = parentTransform * GetMatrix();

				aabb = mesh->data->aabb.Transform(transformedMatrix);

			}

		}

	}

}