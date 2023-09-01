#pragma once

#include "System.h"
#include "Actor.h"
#include "../mesh/Mesh.h"

namespace Atlas {

    namespace Actor {

        class MeshActor : public Actor {

        public:
            void Update(Camera camera, float deltaTime,
                mat4 parentTransform, bool parentUpdate) override;

            ResourceHandle<Mesh::Mesh> mesh;

            mat4 lastGlobalMatrix = mat4(1.0);

        protected:
            explicit MeshActor(const ResourceHandle<Mesh::Mesh>& mesh) : mesh(mesh) {}

        };


    }

}