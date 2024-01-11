#pragma once

#include "../System.h"
#include "../Camera.h"
#include "../mesh/Mesh.h"
#include "../volume/AABB.h"

namespace Atlas {

    namespace Actor {

        class VegetationActor {

        public:
            VegetationActor() = default;

            VegetationActor(const ResourceHandle<Mesh::Mesh>& mesh, vec3 position);

            ~VegetationActor() {}

            vec3 GetPosition() const;

            ResourceHandle<Mesh::Mesh> mesh;

        private:
            vec3 position = vec3(0.0f);

        };

    }

}