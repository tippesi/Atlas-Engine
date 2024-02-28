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

            public:
                MeshComponent() = default;
                explicit MeshComponent(Scene* scene) : scene(scene) {}
                explicit MeshComponent(Scene* scene, const ResourceHandle<Mesh::Mesh>& mesh) : 
                    scene(scene), mesh(mesh) {};

                void ChangeResource(const ResourceHandle<Mesh::Mesh>& mesh);

                ResourceHandle<Mesh::Mesh> mesh = ResourceHandle<Mesh::Mesh>();

                bool visible = true;
                bool dontCull = false;

                Volume::AABB aabb = Volume::AABB{ vec3{-1.0f}, vec3{1.0f} };

            protected:
                Scene* scene = nullptr;

                bool inserted = false;

                friend Scene;

            };

		}

	}

}