#include "SoftBodySimulationMesh.h"
#include "MeshData.h"
#include "MeshSimplifier.h"

#include "../common/MathHelper.h"

#include <numeric>
#include <algorithm>
#include <ranges>
#include <set>
#include <array>

namespace Atlas {

    namespace Mesh {

        SoftBodySimulationMesh::SoftBodySimulationMesh(const MeshData& data, const SoftBodyMeshSettings& settings) {

            auto vertexCount = size_t(data.GetVertexCount() - data.lodVertexCount);
            std::span<const vec3> renderPositions(data.vertices.data.data(), vertexCount);
            auto hasNormals = data.normals.GetElementCount() >= vertexCount;

            std::vector<uint32_t> renderIndices;
            for (const auto& subData : data.subData) {
                auto range = subData.GetLod(0);
                for (uint32_t i = 0; i < range.indicesCount; i++)
                    renderIndices.push_back(data.indices[range.indicesOffset + i]);
            }

            // Render vertices at the same position (UV seams, hard edges) are one simulated vertex
            std::vector<uint32_t> weldedIndices, renderToWelded;
            auto welded = MeshSimplifier::Weld(renderIndices, renderPositions, weldedIndices, renderToWelded);

            // Two sided meshes modelled with back faces have each triangle twice once welded, the simulation keeps one
            std::set<std::array<uint32_t, 3>> weldedTriangles;
            std::vector<uint32_t> uniqueIndices;
            for (size_t i = 0; i + 2 < weldedIndices.size(); i += 3) {
                std::array<uint32_t, 3> triangle = { weldedIndices[i], weldedIndices[i + 1], weldedIndices[i + 2] };
                std::ranges::sort(triangle);
                if (weldedTriangles.insert(triangle).second)
                    uniqueIndices.insert(uniqueIndices.end(), weldedIndices.begin() + i, weldedIndices.begin() + i + 3);
            }
            weldedIndices = uniqueIndices;

            positions = welded;
            indices = weldedIndices;
            std::vector<uint32_t> weldedToSimulated(welded.size());
            std::iota(weldedToSimulated.begin(), weldedToSimulated.end(), 0u);

            if (settings.maxSimulatedVertices > 0 && size_t(settings.maxSimulatedVertices) < welded.size()) {
                SimplifyOptions options = {
                    .targetRatio = float(settings.maxSimulatedVertices) / float(welded.size()),
                    .lockBorder = settings.keepBorders,
                    .regularization = SimplifyRegularization::Full
                };
                auto simplified = MeshSimplifier::Simplify(weldedIndices, welded, options);
                if (!simplified.indices.empty()) {
                    auto count = MeshSimplifier::OptimizeVertexFetchRemap(simplified.indices, welded.size(), weldedToSimulated);
                    positions.resize(count);
                    for (size_t i = 0; i < welded.size(); i++)
                        if (weldedToSimulated[i] != ~0u)
                            positions[weldedToSimulated[i]] = welded[i];
                    indices = simplified.indices;
                    for (auto& index : indices)
                        index = weldedToSimulated[index];
                }
            }

            masses.assign(positions.size(), 0.0f);
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                auto i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
                auto area = 0.5f * glm::length(glm::cross(positions[i1] - positions[i0], positions[i2] - positions[i0]));
                masses[i0] += area / 3.0f;
                masses[i1] += area / 3.0f;
                masses[i2] += area / 3.0f;
            }

            // Vertices removed by the simplification follow the closest point on a simulated triangle, found in a grid
            Volume::AABB bounds(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
            for (const auto& position : positions)
                bounds.Grow(position);

            auto triangleCount = indices.size() / 3;
            float averageExtent = 0.0f;
            for (size_t i = 0; i < triangleCount; i++) {
                Volume::AABB triangleBounds(positions[indices[i * 3]], positions[indices[i * 3]]);
                triangleBounds.Grow(positions[indices[i * 3 + 1]]);
                triangleBounds.Grow(positions[indices[i * 3 + 2]]);
                auto size = triangleBounds.max - triangleBounds.min;
                averageExtent += std::max(size.x, std::max(size.y, size.z));
            }
            averageExtent /= float(std::max(triangleCount, size_t(1)));

            auto boundsSize = triangleCount ? bounds.max - bounds.min : vec3(0.0f);
            auto cellSize = std::max(averageExtent, std::max(boundsSize.x, std::max(boundsSize.y, boundsSize.z)) / 64.0f);
            cellSize = cellSize > 0.0f ? cellSize : 1.0f;
            auto gridSize = glm::max(ivec3(boundsSize / cellSize) + 1, ivec3(1));
            std::vector<std::vector<uint32_t>> cells(size_t(gridSize.x) * gridSize.y * gridSize.z);
            auto getCell = [&](vec3 point) {
                return glm::clamp(ivec3(glm::floor((point - bounds.min) / cellSize)), ivec3(0), gridSize - 1);
                };
            auto getCellIdx = [&](ivec3 cell) {
                return (size_t(cell.z) * gridSize.y + cell.y) * gridSize.x + cell.x;
                };
            for (size_t i = 0; i < triangleCount; i++) {
                auto a = positions[indices[i * 3]], b = positions[indices[i * 3 + 1]], c = positions[indices[i * 3 + 2]];
                auto min = getCell(glm::min(a, glm::min(b, c))), max = getCell(glm::max(a, glm::max(b, c)));
                for (int32_t z = min.z; z <= max.z; z++)
                    for (int32_t y = min.y; y <= max.y; y++)
                        for (int32_t x = min.x; x <= max.x; x++)
                            cells[getCellIdx(ivec3(x, y, z))].push_back(uint32_t(i));
            }

            // Barycentric weights of the closest point on the triangle
            auto closestPoint = [](vec3 p, vec3 a, vec3 b, vec3 c) {
                auto ab = b - a, ac = c - a, ap = p - a;
                auto d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
                if (d1 <= 0.0f && d2 <= 0.0f)
                    return vec3(1.0f, 0.0f, 0.0f);
                auto bp = p - b;
                auto d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
                if (d3 >= 0.0f && d4 <= d3)
                    return vec3(0.0f, 1.0f, 0.0f);
                auto vc = d1 * d4 - d3 * d2;
                if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                    auto v = d1 / (d1 - d3);
                    return vec3(1.0f - v, v, 0.0f);
                }
                auto cp = p - c;
                auto d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
                if (d6 >= 0.0f && d5 <= d6)
                    return vec3(0.0f, 0.0f, 1.0f);
                auto vb = d5 * d2 - d1 * d6;
                if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                    auto w = d2 / (d2 - d6);
                    return vec3(1.0f - w, 0.0f, w);
                }
                auto va = d3 * d6 - d5 * d4;
                if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                    auto w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                    return vec3(0.0f, 1.0f - w, w);
                }
                auto denominator = 1.0f / (va + vb + vc);
                auto v = vb * denominator, w = vc * denominator;
                return vec3(1.0f - v - w, v, w);
                };

            auto bind = [&](vec3 point) {
                Binding binding = { .vertices = { 0, 0, 0 }, .weights = vec3(1.0f, 0.0f, 0.0f), .offset = 0.0f };
                auto bestDistance = std::numeric_limits<float>::max();
                auto center = getCell(point);
                auto maxRing = std::max(gridSize.x, std::max(gridSize.y, gridSize.z));
                for (int32_t ring = 0; ring <= maxRing; ring++) {
                    auto min = glm::max(center - ring, ivec3(0)), max = glm::min(center + ring, gridSize - 1);
                    for (int32_t z = min.z; z <= max.z; z++) {
                        for (int32_t y = min.y; y <= max.y; y++) {
                            for (int32_t x = min.x; x <= max.x; x++) {
                                auto cell = ivec3(x, y, z);
                                // Only the shell of the ring, the inner cells were searched before
                                auto distance = glm::abs(cell - center);
                                if (std::max(distance.x, std::max(distance.y, distance.z)) != ring)
                                    continue;
                                for (auto triangle : cells[getCellIdx(cell)]) {
                                    auto i0 = indices[triangle * 3], i1 = indices[triangle * 3 + 1], i2 = indices[triangle * 3 + 2];
                                    auto weights = closestPoint(point, positions[i0], positions[i1], positions[i2]);
                                    auto closest = weights.x * positions[i0] + weights.y * positions[i1] + weights.z * positions[i2];
                                    auto distance = glm::distance(point, closest);
                                    if (distance >= bestDistance)
                                        continue;
                                    bestDistance = distance;
                                    auto faceNormal = Helper::Math::NormalizeOrZero(glm::cross(positions[i1] - positions[i0],
                                        positions[i2] - positions[i0]));
                                    binding = { .vertices = { i0, i1, i2 }, .weights = weights,
                                        .offset = glm::dot(point - closest, faceNormal) };
                                }
                            }
                        }
                    }
                    // Cells further out are at least this far away
                    if (bestDistance <= float(ring) * cellSize)
                        break;
                }
                return binding;
                };

            std::vector<Binding> weldedBindings(welded.size());
            for (size_t i = 0; i < welded.size(); i++) {
                auto simulated = weldedToSimulated[i];
                if (simulated != ~0u)
                    weldedBindings[i] = { .vertices = { simulated, simulated, simulated },
                        .weights = vec3(1.0f, 0.0f, 0.0f), .offset = 0.0f };
                else
                    weldedBindings[i] = bind(welded[i]);
            }

            bindings.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; i++)
                bindings[i] = renderToWelded[i] != ~0u ? weldedBindings[renderToWelded[i]] : bind(renderPositions[i]);

            // The simulated normals follow the winding, render vertices whose normal points the other way flip them
            if (!hasNormals)
                return;
            std::vector<vec3> restNormals;
            CalculateNormals(positions, restNormals);
            for (size_t i = 0; i < vertexCount; i++) {
                auto& binding = bindings[i];
                vec3 normal(0.0f);
                for (int32_t j = 0; j < 3; j++)
                    normal += binding.weights[j] * restNormals[binding.vertices[j]];
                binding.normalSign = glm::dot(normal, vec3(data.normals[i])) < 0.0f ? -1.0f : 1.0f;
            }

        }

        std::vector<uint32_t> SoftBodySimulationMesh::GetPinnedVertices(const std::vector<Volume::AABB>& pinBounds) const {

            std::vector<uint32_t> pinned;
            for (size_t i = 0; i < positions.size(); i++) {
                if (std::any_of(pinBounds.begin(), pinBounds.end(), [&](const auto& bounds) { return bounds.IsInside(positions[i]); }))
                    pinned.push_back(uint32_t(i));
            }

            // Thin bounds can miss every vertex of a simplified mesh, the body would fall down
            for (const auto& bounds : pinBounds) {
                if (positions.empty() || GetVertexCount(bounds) > 0)
                    continue;
                auto closest = std::ranges::min(std::views::iota(size_t(0), positions.size()), {}, [&](size_t i) {
                    return glm::distance(glm::clamp(positions[i], bounds.min, bounds.max), positions[i]);
                    });
                if (std::ranges::find(pinned, uint32_t(closest)) == pinned.end())
                    pinned.push_back(uint32_t(closest));
            }
            return pinned;

        }

        size_t SoftBodySimulationMesh::GetVertexCount(const Volume::AABB& bounds) const {

            return std::ranges::count_if(positions, [&](const auto& position) { return bounds.IsInside(position); });

        }

        void SoftBodySimulationMesh::CalculateNormals(const std::vector<vec3>& simulatedPositions,
            std::vector<vec3>& simulatedNormals) const {

            simulatedNormals.assign(simulatedPositions.size(), vec3(0.0f));
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                auto i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
                // Not normalized, larger faces weigh more
                auto faceNormal = glm::cross(simulatedPositions[i1] - simulatedPositions[i0],
                    simulatedPositions[i2] - simulatedPositions[i0]);
                simulatedNormals[i0] += faceNormal;
                simulatedNormals[i1] += faceNormal;
                simulatedNormals[i2] += faceNormal;
            }
            for (auto& normal : simulatedNormals)
                normal = Helper::Math::NormalizeOrZero(normal);

        }

        void SoftBodySimulationMesh::GetRenderVertex(size_t idx, const std::vector<vec3>& simulatedPositions,
            const std::vector<vec3>& simulatedNormals, vec3& position, vec3& normal) const {

            const auto& binding = bindings[idx];
            position = vec3(0.0f);
            normal = vec3(0.0f);
            for (int32_t i = 0; i < 3; i++) {
                position += binding.weights[i] * simulatedPositions[binding.vertices[i]];
                normal += binding.weights[i] * simulatedNormals[binding.vertices[i]];
            }
            normal = Helper::Math::NormalizeOrZero(normal);
            position += binding.offset * normal;
            normal *= binding.normalSign;

        }

    }

}
