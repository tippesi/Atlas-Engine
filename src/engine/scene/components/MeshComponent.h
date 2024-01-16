#pragma once

#include "../Entity.h"
#include "../../System.h"

#include "../../mesh/Mesh.h"
#include "../../resource/Resource.h"

namespace Atlas {

	namespace Scene {

        class Scene;

		namespace Components {

            class MeshComponent {

                friend Scene;

            public:
                MeshComponent() = default;
                MeshComponent(const MeshComponent& that) = default;
                explicit MeshComponent(ResourceHandle<Mesh::Mesh> mesh) : mesh(mesh) {}

                const ResourceHandle<Mesh::Mesh> mesh = ResourceHandle<Mesh::Mesh>();

                bool visible = true;
                bool dontCull = false;

            protected:
                bool inserted = false;

            };

		}

	}

}