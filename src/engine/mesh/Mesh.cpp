#include "Mesh.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

#include "../graphics/ASBuilder.h"
#include "../jobsystem/JobSystem.h"

namespace Atlas {

    namespace Mesh {

        std::mutex Mesh::generatingMutex;
        std::vector<Mesh*> Mesh::generatingMeshes;

        Mesh::Mesh(MeshData& meshData, MeshMobility mobility, MeshUsage usage)
            : data(meshData), mobility(mobility), usage(usage) {

            UpdateData();

        }

        Mesh::Mesh(MeshMobility mobility, MeshUsage usage)
            : mobility(mobility), usage(usage) {


        }

        Mesh::~Mesh() {

            JobSystem::Wait(lodJob);

            std::scoped_lock lock(generatingMutex);
            std::erase(generatingMeshes, this);

        }

        void Mesh::UpdateData() {

            data.UpdateUVScales();

            CreateBuffers();
            geometryResident = true;

        }

        void Mesh::CreateBuffers() {

            // The streaming creates the buffers in jobs, while the levels of detail might be generated
            std::scoped_lock lock(dataMutex);

            bool hostAccessible = usage & MeshUsageBits::HostAccessBit;

            if (data.indices.ContainsData()) {
                auto type = data.indices.GetElementSize() == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                if (data.lodIndices.ContainsData()) {
                    // The levels of detail are drawn from the same buffer behind level 0
                    DataComponent<uint32_t> allIndices(data.indices.format);
                    allIndices.data.reserve(size_t(data.GetIndexCount()) + data.lodIndices.GetElementCount());
                    allIndices.data.insert(allIndices.data.end(), data.indices.begin(),
                        data.indices.begin() + data.GetIndexCount());
                    allIndices.data.insert(allIndices.data.end(), data.lodIndices.begin(), data.lodIndices.end());
                    indexBuffer = Buffer::IndexBuffer(type, allIndices.GetElementCount(),
                        allIndices.GetConvertedVoid(), hostAccessible);
                }
                else {
                    indexBuffer = Buffer::IndexBuffer(type, data.GetIndexCount(),
                        data.indices.GetConvertedVoid(), hostAccessible);
                }
            }
            if (data.vertices.ContainsData()) {
                vertexBuffer = Buffer::VertexBuffer(data.vertices.GetFormat(), data.GetVertexCount(),
                    data.vertices.GetConvertedVoid(), hostAccessible);
            }
            if (data.normals.ContainsData()) {
                normalBuffer = Buffer::VertexBuffer(data.normals.GetFormat(), data.GetVertexCount(),
                    data.normals.GetConvertedVoid(), hostAccessible);
            }
            if (data.texCoords.ContainsData()) {
                texCoordBuffer = Buffer::VertexBuffer(data.texCoords.GetFormat(), data.GetVertexCount(),
                    data.texCoords.GetConvertedVoid(), hostAccessible);
            }
            if (data.texCoords1.ContainsData()) {
                texCoord1Buffer = Buffer::VertexBuffer(data.texCoords1.GetFormat(), data.GetVertexCount(),
                    data.texCoords1.GetConvertedVoid(), hostAccessible);
            }
            if (data.tangents.ContainsData()) {
                tangentBuffer = Buffer::VertexBuffer(data.tangents.GetFormat(), data.GetVertexCount(),
                    data.tangents.GetConvertedVoid(), hostAccessible);
            }
            if (data.colors.ContainsData()) {
                colorBuffer = Buffer::VertexBuffer(data.colors.GetFormat(), data.GetVertexCount(),
                    data.colors.GetConvertedVoid(), hostAccessible);
            }
            if (data.IsSkinned()) {
                jointBuffer = Buffer::VertexBuffer(data.joints.GetFormat(), data.GetVertexCount(),
                    data.joints.GetConvertedVoid(), hostAccessible);
                weightBuffer = Buffer::VertexBuffer(data.weights.GetFormat(), data.GetVertexCount(),
                    data.weights.GetConvertedVoid(), hostAccessible);
            }

            UpdateVertexArray();

        }

        void Mesh::UpdatePipelines() {

            for (auto& subData : data.subData) {
                subData.material = data.materials[subData.materialIdx];
                subData.ResetPipelines();
            }

            UpdateRenderBounds();

        }

        Volume::AABB Mesh::GetRenderBounds() const {

            return Volume::AABB(data.aabb.min - vec3(renderBoundsPadding), data.aabb.max + vec3(renderBoundsPadding));

        }

        void Mesh::UpdateRenderBounds() {

            renderBoundsPadding = 0.0f;
            for (const auto& material : data.materials) {
                if (material.IsLoaded() && material->HasGraph())
                    renderBoundsPadding = glm::max(renderBoundsPadding, material->graph->maxDisplacement);
            }

        }

        void Mesh::UpdateVertexArray() {

            vertexArray.AddIndexComponent(indexBuffer);
            vertexArray.AddComponent(0, vertexBuffer);
            if (data.normals.ContainsData())
                vertexArray.AddComponent(1, normalBuffer);
            if (data.texCoords.ContainsData())
                vertexArray.AddComponent(2, texCoordBuffer);
            if (data.tangents.ContainsData())
                vertexArray.AddComponent(3, tangentBuffer);
            if (data.colors.ContainsData())
                vertexArray.AddComponent(4, colorBuffer);
            if (data.IsSkinned()) {
                vertexArray.AddComponent(5, jointBuffer);
                vertexArray.AddComponent(6, weightBuffer);
            }
            if (data.texCoords1.ContainsData())
                vertexArray.AddComponent(7, texCoord1Buffer);

        }

        void Mesh::InvertNormals() {

            if (!data.normals.ContainsData())
                return;

            for (auto& normal : data.normals)
                normal = vec4(-vec3(normal.x, normal.y, normal.z), normal.w);

            UpdateData();

            if (blas)
                blas->isBvhBuilt = false;

        }

        void Mesh::GenerateLods(const MeshLodSettings& settings) {

            // Renderers read the data and the buffers of the current levels until the result is taken over
            JobSystem::Wait(lodJob);
            {
                std::scoped_lock lock(generatingMutex);
                if (!generatingLods)
                    generatingMeshes.push_back(this);
                generatingLods = true;
            }

            JobSystem::Execute(lodJob, [this, settings](JobData&) {
                Ref<MeshData> generated;
                {
                    std::scoped_lock lock(dataMutex);
                    generated = CreateRef<MeshData>(data);
                }
                generated->GenerateLods(settings);
                generatedLods = generated;
                });

        }

        void Mesh::ClearLods() {

            GenerateLods({ .maxLodCount = 1 });

        }

        bool Mesh::IsGeneratingLods() const {

            return generatingLods;

        }

        void Mesh::WaitForLods() {

            JobSystem::Wait(lodJob);

        }

        void Mesh::ApplyGeneratedLods(bool wait) {

            // No job waits under the lock, a waiting thread runs other jobs which might destroy meshes
            std::scoped_lock lock(generatingMutex);
            std::erase_if(generatingMeshes, [wait](Mesh* mesh) {
                if (!mesh->lodJob.HasFinished())
                    return false;

                {
                    // BVH builds hold the data for a while, the next scene update tries again
                    std::unique_lock dataLock(mesh->dataMutex, std::defer_lock);
                    if (wait)
                        dataLock.lock();
                    else if (!dataLock.try_lock())
                        return false;

                    mesh->data.TakeLods(*mesh->generatedLods);
                    mesh->generatedLods = nullptr;
                }
                mesh->generatingLods = false;

                // Buffers which a streaming job created before have the old levels, even if the mesh isn't resident yet
                if (mesh->indexBuffer.buffer)
                    mesh->CreateBuffers();

                // The levels of detail need their own acceleration structures
                if (mesh->blas)
                    mesh->blas->isBvhBuilt = false;
                return true;
                });

        }

        void Mesh::BuildBVH(bool parallelBuild) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            bool hardwareRayTracing = device->support.hardwareRayTracing;

            AE_ASSERT(data.indexCount > 0 && "There is no data in this mesh");

            std::scoped_lock lock(dataMutex);

            if (data.indexCount == 0 || !vertexBuffer.elementCount || !indexBuffer.elementCount) return;

            if (hardwareRayTracing && data.IsSkinned()) {
                std::vector<vec3> restVertices;
                std::vector<vec4> restNormals;
                data.GetRestPoseVertices(restVertices, restNormals);
                restVertexBuffer = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, restVertices.size(), restVertices.data());
            }

            auto buildLod = [&](int32_t lod) {
                std::vector<RayTracing::BLAS::Triangle> triangles;
                data.BuildBVHData(triangles, lod);

                auto lodBlas = CreateRef<RayTracing::BLAS>();
                if (hardwareRayTracing) {
                    std::vector<Graphics::ASGeometryRegion> geometryRegions;
                    geometryRegions.reserve(data.subData.size());

                    for (auto& subData : data.subData) {
                        auto range = subData.GetLod(lod);
                        geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                            .indexCount = range.indicesCount,
                            .indexOffset = range.indicesOffset,
                            .opaque = !subData.material->opacityMap.IsValid() && subData.material->opacity == 1.0f &&
                                !subData.material->HasGraph()
                            });
                    }

                    lodBlas->Build(triangles, data.materials, data.IsSkinned() ? restVertexBuffer : vertexBuffer,
                        indexBuffer, geometryRegions);
                }
                else {
                    lodBlas->Build(triangles, data.materials);
                }
                return lodBlas;
            };

            // Skinned meshes are traced in their rest pose or deformed near the camera, both with level 0
            std::vector<Ref<RayTracing::BLAS>> newLodBlases;
            if (!data.IsSkinned()) {
                for (int32_t lod = 1; lod < data.GetLodCount(); lod++)
                    newLodBlases.push_back(buildLod(lod));
            }

            lodBlases = std::move(newLodBlases);
            blas = buildLod(0);
            blasCreated = true;

        }

        void Mesh::ClearBVH() {

            /*
            // This whole operation can only be done when no ray tracing jobs or bindless updates are running
            isBvhBuilt = false;

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            bool hardwareRayTracing = device->support.hardwareRayTracing;

            AE_ASSERT(data.indexCount > 0 && "There is no data in this mesh");

            if (data.indexCount == 0) return;

            data.gpuTriangles.clear();
            data.gpuTriangles.shrink_to_fit();

            triangleBuffer.Reset();
            if (!hardwareRayTracing) {
                blasNodeBuffer.Reset();
                bvhTriangleBuffer.Reset();
            }
            else {
                triangleOffsetBuffer.Reset();
            }
            */

        }

        bool Mesh::IsBVHBuilt() const {

            return blasCreated && blas && blas->IsBuilt();

        }

        void Mesh::UnloadGeometry() {

            geometryResident = false;

            // The configs point into the vertex array, they are created again on the next draw
            for (auto& subData : data.subData)
                subData.ResetPipelines();

            vertexArray = Buffer::VertexArray();
            indexBuffer = {};
            vertexBuffer = {};
            normalBuffer = {};
            texCoordBuffer = {};
            texCoord1Buffer = {};
            tangentBuffer = {};
            colorBuffer = {};
            jointBuffer = {};
            weightBuffer = {};
            restVertexBuffer = {};

            blasCreated = false;
            blas = nullptr;
            lodBlases.clear();

        }

        bool Mesh::IsGeometryResident() const {

            return geometryResident;

        }

        size_t Mesh::GetGeometrySize() const {

            size_t size = 0;
            auto addSize = [&](const Ref<Graphics::Buffer>& buffer) {
                if (buffer)
                    size += buffer->size;
                };

            addSize(indexBuffer.buffer);
            addSize(vertexBuffer.buffer);
            addSize(normalBuffer.buffer);
            addSize(texCoordBuffer.buffer);
            addSize(texCoord1Buffer.buffer);
            addSize(tangentBuffer.buffer);
            addSize(colorBuffer.buffer);

            return size;

        }

        int32_t Mesh::SelectLod(const mat4& globalMatrix, const Volume::AABB& bounds, vec3 cameraLocation,
            float focalLength, float errorScale, int32_t forcedLod) const {

            if (forcedLod >= 0)
                return std::min(forcedLod, data.GetLodCount() - 1);

            auto pixelError = lodPixelError * errorScale;
            if (data.lodErrors.empty() || pixelError <= 0.0f || focalLength <= 0.0f)
                return 0;

            // Like culling/cull.csh: the distance to the bounds and the largest scale of the instance
            auto distance = glm::distance(glm::clamp(cameraLocation, bounds.min, bounds.max), cameraLocation);
            auto scale = glm::max(glm::length(vec3(globalMatrix[0])),
                glm::max(glm::length(vec3(globalMatrix[1])), glm::length(vec3(globalMatrix[2]))));

            int32_t lod = 0;
            for (size_t i = 0; i < data.lodErrors.size(); i++) {
                if (data.lodErrors[i] * scale * focalLength > pixelError * distance)
                    break;
                lod = int32_t(i) + 1;
            }
            return lod;

        }

    }

}
