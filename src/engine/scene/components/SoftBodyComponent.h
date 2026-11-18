#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"
#include "../../volume/AABB.h"
#include "../../jobsystem/JobGroup.h"

#include "../Entity.h"
#include "MeshComponent.h"
#include "TransformComponent.h"

#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>

#include <vector>
#include <atomic>

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            // Simulates the soft body simulation mesh of the mesh as cloth or a deformable body, its level 0 is drawn
            // from the vertices like joint matrices. Entities without a body draw the mesh like any other
            class SoftBodyComponent : public Physics::Body {

            public:
                SoftBodyComponent() = default;
                SoftBodyComponent(Scene* scene, Entity entity) : entity(entity) {}
                SoftBodyComponent(Scene* scene, Entity entity, const SoftBodyComponent& that);
                SoftBodyComponent(Scene* scene, Entity entity, const Physics::SoftBodySettings& settings)
                    : settings(settings), entity(entity) {}

                // Creates the body again from the rest pose with the current settings
                void Reset();

                // Takes the body out of the simulation, the mesh is drawn in its rest pose until it is created again
                void Rest();

                // Number of simulated vertices, the render vertices are bound to them
                size_t GetSimulatedVertexCount() const { return simulationMesh ? simulationMesh->positions.size() : 0; }

                // Whether the mesh has another simulation mesh than the one of the body
                bool IsOutdated(const MeshComponent& meshComponent) const;

                // Simulated vertices in mesh space, as of the last update in which the body moved
                const std::vector<vec3>& GetSimulatedPositions() const { return positions; }

                // Whether the simulation moved the vertices in the last update
                bool HasMoved() const { return moved; }

                Physics::SoftBodySettings settings;

                // Render vertices of level 0 in mesh space with the packed normal in w
                std::vector<vec4> vertices;
                std::vector<vec4> lastVertices;

            private:
                // The simulation mesh and the Jolt settings are created in a job, bodies which rest keep them
                struct Preparation {
                    vec3 scale = vec3(1.0f);
                    Ref<Mesh::SoftBodySimulationMesh> simulationMesh;
                    std::vector<uint32_t> pinnedVertices;
                    JPH::Ref<JPH::SoftBodySharedSettings> sharedSettings;
                    std::atomic_bool ready = false;
                };

                // Returns true once the body was created, which takes a few updates
                bool InsertIntoPhysicsWorld(const MeshComponent& meshComponent, const TransformComponent& transformComponent,
                    Physics::PhysicsWorld* physicsWorld);

                void RemoveFromPhysicsWorld();

                void MovePinnedVertices(const TransformComponent& transformComponent, float deltaTime);

                bool ReadVertices(const TransformComponent& transformComponent);

                // The last vertices catch up once the body stands still, for the motion vectors
                void SettleHistory();

                Ref<Mesh::SoftBodySimulationMesh> simulationMesh;
                Ref<Preparation> preparation;
                std::vector<uint32_t> pinnedVertices;
                // The pins are moved when the transform changes
                mat4 pinnedMatrix = mat4(1.0f);
                bool moved = false;
                bool pinsMoved = false;
                bool historyPending = false;
                // Bodies the physics world couldn't create (e.g. at its body limit) are only tried again after a reset
                bool creationFailed = false;
                // Whether the GPU scene has the entity as a simulated soft body, it's marked dirty when this changes
                bool drawnSimulated = false;

                // Bounds of the vertices in mesh space
                Volume::AABB bounds;

                // The mesh counts the bodies created from it
                ResourceHandle<Mesh::Mesh> simulatedMesh;

                std::vector<vec3> positions;
                std::vector<vec3> normals;
                std::vector<vec3> pinnedPositions;

                ECS::Entity entity;

                static JobGroup preparationJobs;

                friend Scene;

            };

        }

    }

}
