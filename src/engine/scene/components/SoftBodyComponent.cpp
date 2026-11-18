#include "SoftBodyComponent.h"
#include "../../common/Packing.h"
#include "../../common/MathHelper.h"
#include "../../common/MatrixDecomposition.h"
#include "../../physics/ShapesManager.h"
#include "../../jobsystem/JobSystem.h"

#include <numeric>

namespace Atlas {

    namespace Scene {

        namespace Components {

            JobGroup SoftBodyComponent::preparationJobs{ "Prepare soft bodies", JobPriority::Medium };

            SoftBodyComponent::SoftBodyComponent(Scene* scene, Entity entity, const SoftBodyComponent& that) {

                settings = that.settings;
                vertices = that.vertices;
                lastVertices = that.lastVertices;

                // A copy simulates its own body
                this->entity = entity;

            }

            void SoftBodyComponent::Reset() {

                RemoveFromPhysicsWorld();
                preparation = nullptr;
                creationFailed = false;

            }

            void SoftBodyComponent::Rest() {

                RemoveFromPhysicsWorld();
                vertices.clear();
                lastVertices.clear();

            }

            bool SoftBodyComponent::IsOutdated(const MeshComponent& meshComponent) const {

                return meshComponent.mesh.IsLoaded() && !meshComponent.mesh->IsSoftBodySimulationMesh(simulationMesh);

            }

            bool SoftBodyComponent::InsertIntoPhysicsWorld(const MeshComponent& meshComponent,
                const TransformComponent& transformComponent, Physics::PhysicsWorld* physicsWorld) {

                // Skinned meshes are drawn from their joints, they can't be soft bodies
                const auto& mesh = meshComponent.mesh;
                if (creationFailed || !mesh.IsLoaded() || !mesh->data.indices.ContainsData() || mesh->data.IsSkinned())
                    return false;

                const auto& data = mesh->data;
                auto vertexCount = size_t(data.GetVertexCount() - data.lodVertexCount);
                if (data.normals.GetElementCount() < vertexCount)
                    return false;

                if (preparation && !preparation->ready)
                    return false;

                // Rotated bodies share the Jolt settings, scaled ones can't
                Common::MatrixDecomposition decomposition(transformComponent.globalMatrix);
                if (!preparation || preparation->scale != decomposition.scale ||
                    !mesh->IsSoftBodySimulationMesh(preparation->simulationMesh)) {
                    preparation = CreateRef<Preparation>();
                    preparation->scale = decomposition.scale;
                    JobSystem::Execute(preparationJobs, [preparation = preparation, mesh, settings = settings](JobData&) {
                        auto simulationMesh = mesh->GetSoftBodySimulationMesh();
                        const auto& restPositions = simulationMesh->positions;
                        if (!restPositions.empty()) {
                            Physics::SoftBodyGeometry geometry;
                            geometry.vertices.resize(restPositions.size());
                            geometry.inverseMasses.resize(restPositions.size());
                            geometry.indices = simulationMesh->indices;
                            // Mass by area, so the dense parts of a simplified mesh aren't heavier
                            const auto& masses = simulationMesh->masses;
                            auto averageMass = std::accumulate(masses.begin(), masses.end(), 0.0f) / float(masses.size());
                            for (size_t i = 0; i < restPositions.size(); i++) {
                                geometry.vertices[i] = preparation->scale * restPositions[i];
                                geometry.inverseMasses[i] = 1.0f / (masses[i] > 0.0f ? masses[i] : averageMass);
                            }

                            if (settings.pinned)
                                preparation->pinnedVertices = simulationMesh->GetPinnedVertices(settings.pinBounds);
                            for (auto vertex : preparation->pinnedVertices)
                                geometry.inverseMasses[vertex] = 0.0f;

                            preparation->sharedSettings = Physics::ShapesManager::GetSoftBodySharedSettings(settings, geometry);
                        }
                        preparation->simulationMesh = simulationMesh;
                        preparation->ready = true;
                        });
                    return false;
                }

                if (!preparation->sharedSettings)
                    return false;

                auto body = physicsWorld->CreateSoftBody(settings, preparation->sharedSettings, decomposition.translation,
                    decomposition.quaternion, entity);
                if (body.bodyId.IsInvalid()) {
                    creationFailed = true;
                    return false;
                }

                bodyId = body.bodyId;
                world = physicsWorld;
                simulationMesh = preparation->simulationMesh;
                pinnedVertices = preparation->pinnedVertices;
                pinnedMatrix = transformComponent.globalMatrix;
                pinsMoved = false;

                // Until the first simulation step the mesh is drawn in its rest pose
                vertices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; i++) {
                    auto packedNormal = Common::Packing::PackSignedVector3x10_1x2(vec4(vec3(data.normals[i]), 0.0f));
                    vertices[i] = vec4(data.vertices[i], reinterpret_cast<float&>(packedNormal));
                }
                lastVertices = vertices;
                historyPending = false;
                moved = true;
                bounds = mesh->GetRenderBounds();

                simulatedMesh = mesh;
                simulatedMesh->ChangeSoftBodyCount(1);

                return true;

            }

            void SoftBodyComponent::RemoveFromPhysicsWorld() {

                if (!IsValid())
                    return;

                world->DestroyBody(*this);
                world = nullptr;
                bodyId = Physics::BodyID();

                if (simulatedMesh.IsLoaded())
                    simulatedMesh->ChangeSoftBodyCount(-1);
                simulatedMesh = ResourceHandle<Mesh::Mesh>();

            }

            void SoftBodyComponent::MovePinnedVertices(const TransformComponent& transformComponent, float deltaTime) {

                // Pins which stopped are moved once more, so they lose the velocity of their last move
                auto pinsMove = transformComponent.globalMatrix != pinnedMatrix;
                if (pinnedVertices.empty() || (!pinsMove && !pinsMoved))
                    return;
                pinsMoved = pinsMove;
                pinnedMatrix = transformComponent.globalMatrix;

                pinnedPositions.resize(pinnedVertices.size());
                for (size_t i = 0; i < pinnedVertices.size(); i++)
                    pinnedPositions[i] = vec3(transformComponent.globalMatrix * vec4(simulationMesh->positions[pinnedVertices[i]], 1.0f));

                world->MoveSoftBodyVertices(bodyId, pinnedVertices, pinnedPositions, deltaTime);

            }

            bool SoftBodyComponent::ReadVertices(const TransformComponent& transformComponent) {

                moved = world->GetSoftBodyVertices(bodyId, positions);
                if (!moved) {
                    SettleHistory();
                    return false;
                }

                // Mesh space normals keep their orientation under mirrored transforms
                const auto& inverseMatrix = transformComponent.inverseGlobalMatrix;
                for (auto& position : positions)
                    position = vec3(inverseMatrix * vec4(position, 1.0f));

                simulationMesh->CalculateNormals(positions, normals);

                std::swap(lastVertices, vertices);
                vertices.resize(simulationMesh->bindings.size());
                bounds = Volume::AABB(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
                for (size_t i = 0; i < vertices.size(); i++) {
                    vec3 position, normal;
                    simulationMesh->GetRenderVertex(i, positions, normals, position, normal);
                    bounds.Grow(position);
                    auto packedNormal = Common::Packing::PackSignedVector3x10_1x2(vec4(normal, 0.0f));
                    vertices[i] = vec4(position, reinterpret_cast<float&>(packedNormal));
                }
                if (lastVertices.size() != vertices.size())
                    lastVertices = vertices;
                historyPending = true;

                return true;

            }

            void SoftBodyComponent::SettleHistory() {

                if (!historyPending)
                    return;

                lastVertices = vertices;
                historyPending = false;

            }

        }

    }

}
