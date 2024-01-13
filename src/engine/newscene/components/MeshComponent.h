#pragma once

#include "../Entity.h"
#include "../../System.h"

#include "../../mesh/Mesh.h"
#include "../../resource/Resource.h"

namespace Atlas {

	namespace NewScene {

		namespace Components {

            class MeshComponent {

            public:
                MeshComponent() = default;
                MeshComponent(const MeshComponent& that) = default;
                explicit MeshComponent(ResourceHandle<Mesh::Mesh> mesh) : mesh(mesh) {}

                ResourceHandle<Mesh::Mesh> mesh;

            };

		}

	}

}