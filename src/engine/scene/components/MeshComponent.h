#pragma once

#include <utility>

#include "../../System.h"

#include "../../mesh/Mesh.h"
#include "../../resource/Resource.h"
#include "../Entity.h"

namespace Atlas {

	namespace Scene {

        class Scene;

		namespace Components {

            class MeshComponent {

            public:
                MeshComponent() = default;
                MeshComponent(Scene* scene, Entity entity) : scene(scene), entity(entity) {}
                explicit MeshComponent(Scene* scene, Entity entity, const ResourceHandle<Mesh::Mesh>& mesh) :
                    mesh(mesh), scene(scene), entity(entity) {};
                explicit MeshComponent(Scene* scene, Entity entity, const MeshComponent& that);

                void ChangeResource(const ResourceHandle<Mesh::Mesh>& mesh);

                // These mark the entity dirty, the instances on the GPU are only updated for dirty entities
                void SetVisible(bool visible);

                void SetDontCull(bool dontCull);

                ResourceHandle<Mesh::Mesh> mesh = ResourceHandle<Mesh::Mesh>();

                bool visible = true;
                bool dontCull = false;

                Volume::AABB aabb = Volume::AABB{ vec3{-1.0f}, vec3{1.0f} };

            protected:
                Scene* scene = nullptr;

                bool inserted = false;
                bool resourceChanged = false;

                Entity entity;

                friend Scene;

            };

		}

	}

}