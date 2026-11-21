#include "MeshData.h"

#include "../volume/BVH.h"
#include "../common/MathHelper.h"
#include "../Log.h"
#include "MeshSimplifier.h"

#include <algorithm>
#include <limits>

namespace Atlas {

    namespace Mesh {

        void MeshSubDataPipelines::Set(const PipelineConfig& config) {

            configs[0] = config;
            configs[1] = config.GetReversedWindingVariant();
            configs[2] = PipelineConfig();
            configs[3] = PipelineConfig();
            hasDrawData = configs[0].HasMacro("DRAW_DATA");
            order = invalidOrder;

        }

        void MeshSubDataPipelines::Reset() {

            for (auto& config : configs)
                config = PipelineConfig();
            graphPending = false;
            usesGraph = false;
            graphAnimatesShadow = false;
            graphReadsTexCoords = false;
            hasDrawData = false;
            order = invalidOrder;

        }

        void MeshSubData::ResetPipelines() {

            mainPipelines.Reset();
            shadowPipelines.Reset();

        }

        MeshSubDataLod MeshSubData::GetLod(int32_t lod) const {

            AE_ASSERT(lod >= 0 && lod <= int32_t(lods.size()) && "Invalid level of detail");
            return lod == 0 ? MeshSubDataLod { indicesOffset, indicesCount } : lods[lod - 1];

        }

        MeshData::MeshData(const MeshData& that) {

            DeepCopy(that);

        }

        MeshData::MeshData() {

            indices = DataComponent<uint32_t>(ComponentFormat::UnsignedInt);
            lodIndices = DataComponent<uint32_t>(ComponentFormat::UnsignedInt);

            vertices = DataComponent<vec3>(ComponentFormat::Float);
            texCoords = DataComponent<vec2>(ComponentFormat::HalfFloat);
            texCoords1 = DataComponent<vec2>(ComponentFormat::HalfFloat);
            normals = DataComponent<vec4>(ComponentFormat::PackedNormal);
            tangents = DataComponent<vec4>(ComponentFormat::PackedNormal);
            colors = DataComponent<vec4>(ComponentFormat::PackedColor);
            joints = DataComponent<vec4>(ComponentFormat::UnsignedShort);
            weights = DataComponent<vec4>(ComponentFormat::PackedColor);

        }

        MeshData& MeshData::operator=(const MeshData& that) {

            if (this != &that) {

                DeepCopy(that);

            }

            return *this;

        }

        void MeshData::SetIndexCount(int32_t count) {

            indexCount = count;

        }

        int32_t MeshData::GetIndexCount() const {

            return indexCount;

        }

        void MeshData::SetVertexCount(int32_t count) {

            vertexCount = count;

        }

        int32_t MeshData::GetVertexCount() const {

            return vertexCount;

        }

        void  MeshData::BuildBVHData(std::vector<RayTracing::BLAS::Triangle>& triangles, int32_t lod) {

            // The indices of the levels of detail follow the ones of level 0
            auto getIndex = [&](uint32_t idx) {
                return idx < uint32_t(indexCount) ? indices[idx] : lodIndices[idx - uint32_t(indexCount)];
            };

            uint32_t triangleCount = 0;

            for (auto& sub : subData) {
                triangleCount += sub.GetLod(lod).indicesCount / 3;
            }

            triangles.resize(triangleCount);

            // Skinned meshes are ray traced in their rest pose
            std::vector<vec3> restVertices;
            std::vector<vec4> restNormals;
            if (IsSkinned())
                GetRestPoseVertices(restVertices, restNormals);
            const auto& bvhVertices = IsSkinned() ? restVertices : vertices.Get();
            const auto& bvhNormals = IsSkinned() ? restNormals : normals.Get();

            triangleCount = 0;

            for (auto& sub : subData) {

                auto range = sub.GetLod(lod);
                auto subDataTriangleCount = range.indicesCount / 3;

                for (uint32_t i = 0; i < subDataTriangleCount; i++) {

                    auto k = i + triangleCount;

                    auto idx0 = getIndex(range.indicesOffset + i * 3);
                    auto idx1 = getIndex(range.indicesOffset + i * 3 + 1);
                    auto idx2 = getIndex(range.indicesOffset + i * 3 + 2);

                    // Transform everything
                    triangles[k].v0 = bvhVertices[idx0];
                    triangles[k].v1 = bvhVertices[idx1];
                    triangles[k].v2 = bvhVertices[idx2];

                    triangles[k].n0 = normalize(bvhNormals[idx0]);
                    triangles[k].n1 = normalize(bvhNormals[idx1]);
                    triangles[k].n2 = normalize(bvhNormals[idx2]);

                    if (texCoords.ContainsData()) {
                        triangles[k].uv0 = texCoords.Get()[idx0];
                        triangles[k].uv1 = texCoords.Get()[idx1];
                        triangles[k].uv2 = texCoords.Get()[idx2];
                    }

                    if (colors.ContainsData()) {
                        triangles[k].color0 = colors.Get()[idx0];
                        triangles[k].color1 = colors.Get()[idx1];
                        triangles[k].color2 = colors.Get()[idx2];
                    }
                    else {
                        triangles[k].color0 = vec4(1.0f);
                        triangles[k].color1 = vec4(1.0f);
                        triangles[k].color2 = vec4(1.0f);
                    }

                    triangles[k].materialIdx = sub.materialIdx;
                    triangles[k].opacity = sub.material->opacityMap.IsValid() ? -1.0f : sub.material->opacity;

                }

                triangleCount += subDataTriangleCount;

            }

        }

        bool MeshData::IsSkinned() const {

            return !skeleton.joints.empty() && joints.GetElementCount() > 0;

        }

        void MeshData::GetRestPoseVertices(std::vector<vec3>& restVertices, std::vector<vec4>& restNormals) const {

            const auto& restJointMatrices = skeleton.restJointMatrices;
            restVertices.resize(vertices.GetElementCount());
            restNormals.resize(normals.GetElementCount());

            for (size_t i = 0; i < restVertices.size(); i++) {
                auto vertexJoints = ivec4(joints[i]);
                auto vertexWeights = weights[i] / glm::dot(weights[i], vec4(1.0f));
                auto skinMatrix = glm::transpose(vertexWeights.x * restJointMatrices[vertexJoints.x] +
                    vertexWeights.y * restJointMatrices[vertexJoints.y] +
                    vertexWeights.z * restJointMatrices[vertexJoints.z] +
                    vertexWeights.w * restJointMatrices[vertexJoints.w]);

                restVertices[i] = skinMatrix * vec4(vertices[i], 1.0f);
                if (i < restNormals.size())
                    restNormals[i] = vec4(skinMatrix * vec4(vec3(normals[i]), 0.0f), 0.0f);
            }

        }

        const MeshSubData* MeshData::GetSubDataOfTriangle(int32_t triangleIndex) const {

            if (triangleIndex < 0)
                return nullptr;

            auto index = uint32_t(triangleIndex) * 3;
            for (const auto& sub : subData) {
                if (index >= sub.indicesOffset && index < sub.indicesOffset + sub.indicesCount)
                    return &sub;
            }

            return nullptr;

        }

        void MeshData::GenerateLods(const MeshLodSettings& settings) {

            ClearLods();
            lodIndices.SetType(indices.format);
            lodSettings = settings;

            auto lodCount = std::min(settings.maxLodCount, maxLodCount);
            if (lodCount < 2 || primitiveType != 0 || !indices.ContainsData() || !vertices.ContainsData())
                return;

            std::span<const vec3> positions = vertices.Get();
            auto vertexCount = positions.size();

            // All sub data are simplified together, the seams between them stay closed. Every simplified triangle
            // belongs to the sub data of its first vertex, which needs separate vertices per sub data like imports have
            std::vector<uint32_t> allIndices;
            std::vector<uint32_t> vertexSubData(vertexCount, std::numeric_limits<uint32_t>::max());
            for (size_t i = 0; i < subData.size(); i++) {
                const auto& sub = subData[i];
                for (uint32_t j = sub.indicesOffset; j < sub.indicesOffset + sub.indicesCount; j++) {
                    auto& vertexSub = vertexSubData[indices[j]];
                    if (vertexSub != uint32_t(i) && vertexSub != std::numeric_limits<uint32_t>::max()) {
                        Log::Warning("Mesh " + name + " has no levels of detail, its sub data share vertices");
                        return;
                    }
                    vertexSub = uint32_t(i);
                    allIndices.push_back(indices[j]);
                }
            }

            // Seams of the texture coordinates and between sub data are kept. Normal seams may collapse, flat shaded
            // meshes have them at every vertex
            auto positionRemap = MeshSimplifier::GeneratePositionRemap(positions);
            std::vector<SimplifyVertexLock> vertexLock(vertexCount, SimplifyVertexLock::None);
            for (size_t i = 0; i < vertexCount; i++) {
                auto first = positionRemap[i];
                if (vertexSubData[i] != vertexSubData[first] ||
                    (texCoords.GetElementCount() == vertexCount && texCoords[i] != texCoords[first]))
                    vertexLock[first] = SimplifyVertexLock::Protect;
            }
            for (size_t i = 0; i < vertexCount; i++)
                vertexLock[i] = vertexLock[positionRemap[i]];

            // Within the surfaces the attributes add to the error: texture coordinates by the mesh units their texture
            // covers, so the error stays a distance that the culling can compare with the pixels it covers
            const size_t attributeCount = 5;
            std::vector<float> attributes(vertexCount * attributeCount, 0.0f);
            auto hasNormals = normals.GetElementCount() == vertexCount;
            auto hasTexCoords = texCoords.GetElementCount() == vertexCount;
            UpdateUVScales();
            float uvScale = 0.0f;
            for (const auto& sub : subData)
                uvScale = std::max(uvScale, sub.uvScale);
            for (size_t i = 0; i < vertexCount; i++) {
                auto attribute = attributes.data() + i * attributeCount;
                if (hasNormals) {
                    auto normal = Helper::Math::NormalizeOrZero(vec3(normals[i]));
                    attribute[0] = normal.x;
                    attribute[1] = normal.y;
                    attribute[2] = normal.z;
                }
                if (hasTexCoords) {
                    attribute[3] = texCoords[i].x;
                    attribute[4] = texCoords[i].y;
                }
            }
            // The weights are relative to the extent of the mesh
            auto scale = MeshSimplifier::GetScale(positions);
            auto uvWeight = scale > 0.0f ? uvScale / scale : 0.0f;
            const float attributeWeights[attributeCount] = { 0.1f, 0.1f, 0.1f, uvWeight, uvWeight };
            SimplifyOptions simplifyOptions = {
                .absoluteError = true,
                .permissive = true,
                .attributes = attributes,
                .attributeWeights = attributeWeights,
                .attributeCount = attributeCount,
                .vertexLocks = vertexLock
            };

            // The levels get normals of their simplified surfaces, with the smallest angle level 0 has a hard edge at.
            // Vertices whose normals level 0 doesn't derive from its surfaces, e.g. of leaf cards, keep theirs
            float creaseAngle = glm::pi<float>();
            std::vector<bool> surfaceNormals;
            if (hasNormals) {
                std::vector<float> splitAngles;
                for (size_t i = 0; i < vertexCount; i++) {
                    auto first = positionRemap[i];
                    auto angle = glm::acos(glm::clamp(glm::dot(Helper::Math::NormalizeOrZero(vec3(normals[i])),
                        Helper::Math::NormalizeOrZero(vec3(normals[first]))), -1.0f, 1.0f));
                    if (first != i && angle > glm::radians(1.0f))
                        splitAngles.push_back(angle);
                }
                // A few splits are noise, e.g. separate parts touching each other
                if (!splitAngles.empty()) {
                    std::ranges::sort(splitAngles);
                    creaseAngle = 0.9f * splitAngles[splitAngles.size() / 20];
                }

                auto cornerNormals = MeshSimplifier::GenerateCornerNormals(allIndices, positions, creaseAngle);
                surfaceNormals.assign(vertexCount, true);
                for (size_t i = 0; i < allIndices.size(); i++) {
                    auto vertex = allIndices[i];
                    if (glm::dot(Helper::Math::NormalizeOrZero(vec3(normals[vertex])), cornerNormals[i]) < glm::cos(glm::radians(15.0f)))
                        surfaceNormals[vertex] = false;
                }
            }

            std::vector<std::vector<uint32_t>> subDataIndices(subData.size());
            // Each level gets copies of the vertices it uses in the order they are fetched, which is faster to draw
            // than indices spread over all vertices of level 0
            std::vector<uint32_t> levelIndices, cornerVertices, remap;
            std::vector<uint32_t> cornerSources;
            std::vector<vec3> levelNormals;
            std::vector<uint32_t> appendedVertices;
            // Zero keeps the normal of the source vertex
            std::vector<vec3> appendedNormals;
            auto previousCount = allIndices.size();
            // Each level is simplified from level 0, so its error is the one to the full detail
            for (int32_t lod = 1; lod < lodCount; lod++) {
                simplifyOptions.targetIndexCount = size_t(float(previousCount) * settings.reduction);
                auto simplified = MeshSimplifier::Simplify(allIndices, positions, simplifyOptions);
                auto count = simplified.indices.size();
                if (count == 0 || float(count) > float(previousCount) * settings.minReduction)
                    break;

                for (auto& triangles : subDataIndices)
                    triangles.clear();
                for (size_t i = 0; i < count; i += 3) {
                    auto& triangles = subDataIndices[vertexSubData[simplified.indices[i]]];
                    triangles.insert(triangles.end(), simplified.indices.begin() + i, simplified.indices.begin() + i + 3);
                }

                for (auto& triangles : subDataIndices)
                    MeshSimplifier::OptimizeVertexCache(triangles, vertexCount);

                levelIndices.clear();
                for (const auto& triangles : subDataIndices)
                    levelIndices.insert(levelIndices.end(), triangles.begin(), triangles.end());
                // Corners of a vertex share a new vertex as long as their normals are the same
                size_t cornerVertexCount = vertexCount;
                cornerVertices = levelIndices;
                if (hasNormals) {
                    levelNormals = MeshSimplifier::GenerateCornerNormals(levelIndices, positions, creaseAngle);
                    for (size_t i = 0; i < levelIndices.size(); i++) {
                        if (!surfaceNormals[levelIndices[i]])
                            levelNormals[i] = vec3(0.0f);
                    }
                    cornerVertexCount = MeshSimplifier::GenerateCornerVertices(levelIndices, levelNormals, cornerVertices);
                }
                cornerSources.resize(cornerVertexCount);
                for (size_t i = 0; i < levelIndices.size(); i++)
                    cornerSources[cornerVertices[i]] = uint32_t(i);

                auto uniqueCount = MeshSimplifier::OptimizeVertexFetchRemap(cornerVertices, cornerVertexCount, remap);

                auto firstVertex = uint32_t(vertexCount + appendedVertices.size());
                appendedVertices.resize(appendedVertices.size() + uniqueCount);
                appendedNormals.resize(appendedVertices.size(), vec3(0.0f));
                for (size_t i = 0; i < cornerVertexCount; i++) {
                    if (remap[i] == ~0u)
                        continue;
                    auto corner = cornerSources[i];
                    auto appendedIdx = firstVertex - vertexCount + remap[i];
                    appendedVertices[appendedIdx] = levelIndices[corner];
                    if (hasNormals)
                        appendedNormals[appendedIdx] = levelNormals[corner];
                }
                size_t corner = 0;
                for (auto& triangles : subDataIndices) {
                    for (auto& idx : triangles)
                        idx = firstVertex + remap[cornerVertices[corner++]];
                }

                // The new indices go behind the ones of level 0 and the levels before
                for (size_t i = 0; i < subData.size(); i++) {
                    auto& triangles = subDataIndices[i];
                    subData[i].lods.push_back({
                        .indicesOffset = uint32_t(indexCount + lodIndices.GetElementCount()),
                        .indicesCount = uint32_t(triangles.size())
                    });
                    lodIndices.data.insert(lodIndices.data.end(), triangles.begin(), triangles.end());
                }

                lodErrors.push_back(simplified.error);
                previousCount = count;
            }

            if (appendedVertices.empty())
                return;

            auto appendVertices = [&](auto& component) {
                if (component.GetElementCount() != vertexCount)
                    return;
                component.data.reserve(vertexCount + appendedVertices.size());
                for (auto source : appendedVertices) {
                    auto value = component.data[source];
                    component.data.push_back(value);
                }
            };
            appendVertices(vertices);
            appendVertices(texCoords);
            appendVertices(texCoords1);
            appendVertices(normals);
            for (size_t i = 0; hasNormals && i < appendedNormals.size(); i++) {
                auto& normal = normals.data[vertexCount + i];
                if (appendedNormals[i] != vec3(0.0f))
                    normal = vec4(appendedNormals[i], normal.w);
            }
            appendVertices(tangents);
            appendVertices(colors);
            appendVertices(joints);
            appendVertices(weights);

            lodVertexCount = int32_t(appendedVertices.size());
            SetVertexCount(int32_t(vertexCount) + lodVertexCount);
            if (GetVertexCount() > 65535) {
                indices.SetType(ComponentFormat::UnsignedInt);
                lodIndices.SetType(ComponentFormat::UnsignedInt);
            }

        }

        void MeshData::ClearLods() {

            lodIndices.Clear();
            lodErrors.clear();
            for (auto& sub : subData)
                sub.lods.clear();

            if (!lodVertexCount)
                return;

            // The vertices of the levels are behind the ones of level 0
            auto levelZeroCount = size_t(vertexCount - lodVertexCount);
            auto removeVertices = [&](auto& component) {
                if (component.GetElementCount() == size_t(vertexCount))
                    component.data.resize(levelZeroCount);
            };
            removeVertices(vertices);
            removeVertices(texCoords);
            removeVertices(texCoords1);
            removeVertices(normals);
            removeVertices(tangents);
            removeVertices(colors);
            removeVertices(joints);
            removeVertices(weights);

            SetVertexCount(int32_t(levelZeroCount));
            lodVertexCount = 0;

        }

        int32_t MeshData::GetLodCount() const {

            return int32_t(lodErrors.size()) + 1;

        }

        void MeshData::TakeLods(MeshData& generated) {

            AE_ASSERT(generated.subData.size() == subData.size() && "The generated data is no copy of this data");

            // The levels change the vertex data and the index format
            indices = std::move(generated.indices);
            vertices = std::move(generated.vertices);
            texCoords = std::move(generated.texCoords);
            texCoords1 = std::move(generated.texCoords1);
            normals = std::move(generated.normals);
            tangents = std::move(generated.tangents);
            colors = std::move(generated.colors);
            joints = std::move(generated.joints);
            weights = std::move(generated.weights);

            lodIndices = std::move(generated.lodIndices);
            lodErrors = std::move(generated.lodErrors);
            lodVertexCount = generated.lodVertexCount;
            lodSettings = generated.lodSettings;
            vertexCount = generated.vertexCount;
            for (size_t i = 0; i < subData.size(); i++)
                subData[i].lods = std::move(generated.subData[i].lods);

        }

        void MeshData::UpdateUVScales() {

            if (!indices.ContainsData() || !vertices.ContainsData() || !texCoords.ContainsData())
                return;

            for (auto& sub : subData) {
                float area = 0.0f, uvArea = 0.0f;
                for (uint32_t i = sub.indicesOffset; i + 2 < sub.indicesOffset + sub.indicesCount; i += 3) {
                    auto i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
                    area += glm::length(glm::cross(vertices[i1] - vertices[i0], vertices[i2] - vertices[i0]));
                    auto uv1 = texCoords[i1] - texCoords[i0], uv2 = texCoords[i2] - texCoords[i0];
                    uvArea += glm::abs(uv1.x * uv2.y - uv1.y * uv2.x);
                }
                sub.uvScale = uvArea > 0.0f ? glm::sqrt(area / uvArea) : 0.0f;
            }

        }

        void MeshData::DeepCopy(const MeshData& that) {

            name = that.name;

            indices = that.indices;
            lodIndices = that.lodIndices;
            lodErrors = that.lodErrors;
            lodVertexCount = that.lodVertexCount;
            lodSettings = that.lodSettings;

            vertices = that.vertices;
            texCoords = that.texCoords;
            texCoords1 = that.texCoords1;
            normals = that.normals;
            tangents = that.tangents;
            colors = that.colors;
            joints = that.joints;
            weights = that.weights;

            skeleton = that.skeleton;

            indexCount = that.indexCount;
            vertexCount = that.vertexCount;

            primitiveType = that.primitiveType;

            aabb = that.aabb;

            materials.clear();
            subData.clear();

            animations = that.animations;

            materials.resize(that.materials.size());
            subData.resize(that.subData.size());

            // We need to refresh the pointers in the sub data
            for (size_t i = 0; i < that.subData.size(); i++) {
                materials[i] = that.materials[i];
                subData[i] = that.subData[i];
                subData[i].material = materials[i];
            }

        }

    }

}