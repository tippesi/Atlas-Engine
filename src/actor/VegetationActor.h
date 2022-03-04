#ifndef AE_VEGETATIONACTOR_H
#define AE_VEGETATIONACTOR_H

#include "../System.h"
#include "../Camera.h"
#include "../volume/AABB.h"
#include "../mesh/VegetationMesh.h"

namespace Atlas {

    namespace Actor {

        class VegetationActor {

        public:
            VegetationActor() = default;

            VegetationActor(Mesh::VegetationMesh* mesh, vec3 position);

            ~VegetationActor() {}

            vec3 GetPosition() const;

            Mesh::VegetationMesh* mesh = nullptr;

        private:
            vec3 position = vec3(0.0f);

        };

    }

}


#endif
