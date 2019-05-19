#ifndef AE_MOVABLEMESHACTOR_H
#define AE_MOVABLEMESHACTOR_H

#include "MeshActor.h"

namespace Atlas {

    namespace Actor {

        class MovableMeshActor : public MeshActor {

        public:
			MovableMeshActor() : MeshActor(nullptr) {}

            MovableMeshActor(Mesh::Mesh* mesh) : MeshActor(mesh) {}

        };

    }

}

#endif