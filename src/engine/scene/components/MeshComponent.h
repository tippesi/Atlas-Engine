#pragma once

#include <utility>

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
                explicit MeshComponent(ResourceHandle<Mesh::Mesh> mesh) : mesh(std::move(mesh)) {}

                ResourceHandle<Mesh::Mesh> mesh = ResourceHandle<Mesh::Mesh>();

                bool visible = true;
                bool dontCull = false;

                Volume::AABB aabb = Volume::AABB{ vec3{-1.0f}, vec3{1.0f} };

            protected:
                bool inserted = false;

            };

		}

	}

}