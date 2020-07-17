#ifndef AE_MOVABLEMESHACTOR_H
#define AE_MOVABLEMESHACTOR_H

#include "MeshActor.h"

namespace Atlas {

    namespace Actor {

        class MovableMeshActor : public MeshActor {

        public:
			MovableMeshActor() : MeshActor(nullptr) {}

			explicit MovableMeshActor(Mesh::Mesh* mesh, mat4 matrix = mat4(1.0f))
				: MeshActor(mesh) { this->SetMatrix(matrix); }

        };

    }

}

#endif