#include "VegetationActor.h"

namespace Atlas {

    namespace Actor {

        VegetationActor::VegetationActor(Mesh::VegetationMesh* mesh, vec3 position) : mesh(mesh), position(position) {



        }

        vec3 VegetationActor::GetPosition() const {

            return position;
        
        }

    }

}