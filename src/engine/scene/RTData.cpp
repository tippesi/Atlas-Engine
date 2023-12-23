#include "RTData.h"
#include "Scene.h"

#include "../mesh/MeshData.h"
#include "../volume/BVH.h"
#include "../graphics/ASBuilder.h"
#include "../common/ColorConverter.h"

#include <unordered_map>
#include <set>

namespace Atlas {

    namespace Scene {

        RTData::RTData(Scene* scene) : scene(scene) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            hardwareRayTracing = device->support.hardwareRayTracing;

            triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUTriangle));
            bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHTriangle));
            blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHNode));
            geometryTriangleOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));

            auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit |
                Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit;

            materialBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUMaterial));
            bvhInstanceBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHInstance));
            tlasNodeBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHNode));

        }

        void RTData::Build() {

            isValid = false;

            if (hardwareRayTracing) {
                BuildForHardwareRayTracing();
            }
            else {
                BuildForSoftwareRayTracing();
            }

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, true);

            isValid = true;

        }

        void RTData::Update(bool updateTriangleLights) {

            auto actors = scene->GetMeshActors();

            if (!actors.size()) return;

            UpdateMaterials();

            std::vector<GPUBVHInstance> gpuBvhInstances;
            std::vector<Volume::AABB> actorAABBs;

            for (auto& [_, meshInfo] : meshInfos) {
                meshInfo.instanceIndices.clear();
                meshInfo.matrices.clear();
            }

            for (auto& actor : actors) {
                if (!actor->mesh.IsLoaded())
                    continue;

                if (!meshInfos.contains(actor->mesh.GetID()))
                    continue;

                actorAABBs.push_back(actor->aabb);
                auto& meshInfo = meshInfos[actor->mesh.GetID()];
                meshInfo.offset = int32_t(scene->bufferToBindlessIdx[actor->mesh->blasNodeBuffer.Get()]);

                auto inverseMatrix = mat3x4(glm::transpose(actor->inverseGlobalMatrix));

                GPUBVHInstance gpuBvhInstance = {
                    .inverseMatrix = inverseMatrix,
                    .meshOffset = meshInfo.offset,
                    .materialOffset = meshInfo.materialOffset
                };

                meshInfo.matrices.push_back(actor->globalMatrix);
                meshInfo.instanceIndices.push_back(uint32_t(gpuBvhInstances.size()));
                gpuBvhInstances.push_back(gpuBvhInstance);
            }

            if (!gpuBvhInstances.size())
                return;

            if (hardwareRayTracing) {
                UpdateForHardwareRayTracing(actors);
            }
            else {
                gpuBvhInstances = UpdateForSoftwareRayTracing(gpuBvhInstances, actorAABBs);
            }

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, false);

            if (updateTriangleLights)
                UpdateTriangleLights();

            bvhInstanceBuffer.SetSize(gpuBvhInstances.size());
            bvhInstanceBuffer.SetData(gpuBvhInstances.data(), 0, gpuBvhInstances.size());

        }

        void RTData::UpdateMaterials(bool updateTextures) {

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials, updateTextures);

        }

        void RTData::UpdateMaterials(std::vector<GPUMaterial>& materials, bool updateTextures) {

            std::lock_guard lock(mutex);

            auto actors = scene->GetMeshActors();

            auto meshes = scene->GetMeshes();
            materials.clear();

            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                if (!meshInfos.contains(mesh.GetID()))
                    continue;

                auto& meshInfo = meshInfos[mesh.GetID()];
                meshInfo.materialOffset = int32_t(materials.size());

                for (auto& material : mesh->data.materials) {
                    GPUMaterial gpuMaterial;

                    gpuMaterial.baseColor = Common::ColorConverter::ConvertSRGBToLinear(material->baseColor);
                    gpuMaterial.emissiveColor = Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor)
                        * material->emissiveIntensity;

                    gpuMaterial.opacity = material->opacity;

                    gpuMaterial.roughness = material->roughness;
                    gpuMaterial.metalness = material->metalness;
                    gpuMaterial.ao = material->ao;

                    gpuMaterial.reflectance = material->reflectance;

                    gpuMaterial.normalScale = material->normalScale;

                    gpuMaterial.invertUVs = mesh->invertUVs ? 1 : 0;
                    gpuMaterial.twoSided = material->twoSided ? 1 : 0;
                    gpuMaterial.useVertexColors = material->vertexColors ? 1 : 0;

                    if (material->HasBaseColorMap()) {
                        gpuMaterial.baseColorTexture = scene->textureToBindlessIdx[material->baseColorMap];
                    }

                    if (material->HasOpacityMap()) {
                        gpuMaterial.opacityTexture = scene->textureToBindlessIdx[material->opacityMap];
                    }

                    if (material->HasNormalMap()) {
                        gpuMaterial.normalTexture = scene->textureToBindlessIdx[material->normalMap];
                    }

                    if (material->HasRoughnessMap()) {
                        gpuMaterial.roughnessTexture = scene->textureToBindlessIdx[material->roughnessMap];
                    }

                    if (material->HasMetalnessMap()) {
                        gpuMaterial.metalnessTexture = scene->textureToBindlessIdx[material->metalnessMap];
                    }

                    if (material->HasAoMap()) {
                        gpuMaterial.aoTexture = scene->textureToBindlessIdx[material->aoMap];
                    }

                    materials.push_back(gpuMaterial);
                }
            }

            materialBuffer.SetSize(materials.size());
            materialBuffer.SetData(materials.data(), 0, materials.size());

        }

        void RTData::Clear() {

            isValid = false;

        }

        bool RTData::IsValid() {

            return isValid;

        }

        void RTData::BuildForSoftwareRayTracing() {

            std::vector<GPUTriangle> gpuTriangles;
            std::vector<GPUBVHTriangle> gpuBvhTriangles;
            std::vector<GPUBVHNode> gpuBvhNodes;

            auto meshes = scene->GetMeshes();

            int32_t materialCount = 0;
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                // Not all meshes might have a bvh
                if (!mesh->data.gpuTriangles.size())
                    continue;

                auto triangleOffset = int32_t(gpuTriangles.size());
                auto nodeOffset = int32_t(gpuBvhNodes.size());

                for (size_t i = 0; i < mesh->data.gpuBvhNodes.size(); i++) {
                    auto gpuBvhNode = mesh->data.gpuBvhNodes[i];

                    auto leftPtr = gpuBvhNode.leftPtr;
                    auto rightPtr = gpuBvhNode.rightPtr;

                    gpuBvhNode.leftPtr = leftPtr < 0 ? ~((~leftPtr) + triangleOffset) : leftPtr + nodeOffset;
                    gpuBvhNode.rightPtr = rightPtr < 0 ? ~((~rightPtr) + triangleOffset) : rightPtr + nodeOffset;

                    gpuBvhNodes.push_back(gpuBvhNode);
                }

                // Subtract and reassign material offset
                for (size_t i = 0; i < mesh->data.gpuTriangles.size(); i++) {
                    auto gpuTriangle = mesh->data.gpuTriangles[i];
                    auto gpuBvhTriangle = mesh->data.gpuBvhTriangles[i];

                    auto localMaterialIdx = reinterpret_cast<int32_t&>(gpuTriangle.d0.w);
                    auto materialIdx = localMaterialIdx + materialCount;

                    gpuTriangle.d0.w = reinterpret_cast<float&>(materialIdx);
                    gpuBvhTriangle.v1.w = reinterpret_cast<float&>(materialIdx);

                    gpuTriangles.push_back(gpuTriangle);
                    gpuBvhTriangles.push_back(gpuBvhTriangle);
                }

                auto bufferOffset = scene->bufferToBindlessIdx[mesh->blasNodeBuffer.Get()];

                MeshInfo meshInfo = {
                    .offset = int32_t(bufferOffset),
                    .materialOffset = triangleOffset
                };
                meshInfos[mesh.GetID()] = meshInfo;

                BuildTriangleLightsForMesh(mesh);

            }

            if (!gpuTriangles.size())
                return;

            // Upload triangles
            triangleBuffer.SetSize(gpuTriangles.size());
            triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

            bvhTriangleBuffer.SetSize(gpuBvhTriangles.size());
            bvhTriangleBuffer.SetData(gpuBvhTriangles.data(), 0, gpuBvhTriangles.size());

            blasNodeBuffer.SetSize(gpuBvhNodes.size());
            blasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());

        }

        void RTData::BuildForHardwareRayTracing() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            std::vector<uint32_t> triangleOffsets;
            std::vector<GPUTriangle> gpuTriangles;

            Graphics::ASBuilder asBuilder;

            auto meshes = scene->GetMeshes();

            blases.clear();

            int32_t materialCount = 0;
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                // Not all meshes might have a bvh
                if (!mesh->data.gpuTriangles.size())
                    continue;

                auto triangleOffset = int32_t(gpuTriangles.size());
                auto offset = int32_t(triangleOffsets.size());

                // Subtract and reassign material offset
                for (size_t i = 0; i < mesh->data.gpuTriangles.size(); i++) {
                    auto gpuTriangle = mesh->data.gpuTriangles[i];

                    auto localMaterialIdx = reinterpret_cast<int32_t&>(gpuTriangle.d0.w);
                    auto materialIdx = localMaterialIdx + materialCount;

                    gpuTriangle.d0.w = reinterpret_cast<float&>(materialIdx);

                    gpuTriangles.push_back(gpuTriangle);
                }

                std::vector<Graphics::ASGeometryRegion> geometryRegions;
                for (auto& subData : mesh->data.subData) {
                    geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                        .indexCount = subData.indicesCount,
                        .indexOffset = subData.indicesOffset,
                        .opaque = !subData.material->HasOpacityMap() && subData.material->opacity == 1.0f
                        });
                }

                auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(mesh->vertexBuffer.buffer, mesh->indexBuffer.buffer,
                    mesh->vertexBuffer.elementCount, mesh->vertexBuffer.elementSize,
                    mesh->indexBuffer.elementSize, geometryRegions);

                blases.push_back(device->CreateBLAS(blasDesc));

                MeshInfo meshInfo = {
                    .blas = blases.back(),

                    .offset = offset,
                    .materialOffset = triangleOffset
                };
                meshInfos[mesh.GetID()] = meshInfo;

                for (auto& subData : mesh->data.subData) {
                    auto totalTriangleOffset = triangleOffset + subData.indicesOffset / 3;
                    triangleOffsets.push_back(totalTriangleOffset);
                }

                BuildTriangleLightsForMesh(mesh);

            }

            if (!gpuTriangles.size())
                return;

            // Upload triangles
            triangleBuffer.SetSize(gpuTriangles.size());
            triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

            geometryTriangleOffsetBuffer.SetSize(triangleOffsets.size());
            geometryTriangleOffsetBuffer.SetData(triangleOffsets.data(), 0, triangleOffsets.size());

            asBuilder.BuildBLAS(blases);

        }

        std::vector<GPUBVHInstance> RTData::UpdateForSoftwareRayTracing(std::vector<GPUBVHInstance>& gpuBvhInstances,
            std::vector<Volume::AABB>& actorAABBs) {

            auto bvh = Volume::BVH(actorAABBs);

            auto& nodes = bvh.GetTree();
            auto gpuBvhNodes = std::vector<GPUBVHNode>(nodes.size());
            // Copy to GPU format
            for (size_t i = 0; i < nodes.size(); i++) {
                gpuBvhNodes[i].leftPtr = nodes[i].leftPtr;
                gpuBvhNodes[i].rightPtr = nodes[i].rightPtr;

                gpuBvhNodes[i].leftAABB.min = nodes[i].leftAABB.min;
                gpuBvhNodes[i].leftAABB.max = nodes[i].leftAABB.max;

                gpuBvhNodes[i].rightAABB.min = nodes[i].rightAABB.min;
                gpuBvhNodes[i].rightAABB.max = nodes[i].rightAABB.max;
            }

            // Order after the BVH build to fit the node indices
            std::vector<GPUBVHInstance> orderedGpuBvhInstances(bvh.refs.size());
            for (size_t i = 0; i < bvh.refs.size(); i++) {
                orderedGpuBvhInstances[i] = gpuBvhInstances[bvh.refs[i].idx];
            }

            tlasNodeBuffer.SetSize(gpuBvhNodes.size());
            tlasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());

            return orderedGpuBvhInstances;

        }

        void RTData::UpdateForHardwareRayTracing(std::vector<Actor::MeshActor*>& actors) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            Graphics::ASBuilder asBuilder;

            std::vector<VkAccelerationStructureInstanceKHR> instances;

            for (auto actor : actors) {
                if (!actor->mesh.IsLoaded())
                    continue;

                if (!meshInfos.contains(actor->mesh.GetID()))
                    continue;

                auto& meshInfo = meshInfos[actor->mesh.GetID()];

                VkAccelerationStructureInstanceKHR inst = {};
                VkTransformMatrixKHR transform;

                auto transposed = glm::transpose(actor->globalMatrix);
                std::memcpy(&transform, &transposed, sizeof(VkTransformMatrixKHR));

                inst.transform = transform;
                inst.instanceCustomIndex = meshInfo.offset;
                inst.accelerationStructureReference = meshInfo.blas->bufferDeviceAddress;
                inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                inst.mask = 0xFF;
                inst.instanceShaderBindingTableRecordOffset = 0;
                instances.push_back(inst);
            }

            auto tlasDesc = Graphics::TLASDesc();
            tlas = device->CreateTLAS(tlasDesc);

            asBuilder.BuildTLAS(tlas, instances);

        }

        void RTData::BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh> &mesh) {

            auto& gpuTriangles = mesh->data.gpuTriangles;
            auto& materials = mesh->data.materials;

            auto& meshInfo = meshInfos[mesh.GetID()];

            // Triangle lights
            for (size_t i = 0; i < gpuTriangles.size(); i++) {
                auto& triangle = gpuTriangles[i];
                auto idx = reinterpret_cast<int32_t&>(triangle.d0.w);
                auto& material = materials[idx];

                auto radiance = Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor)
                    * material->emissiveIntensity;
                auto brightness = dot(radiance, vec3(0.3333f));

                if (brightness > 0.0f) {
                    // Extract normal information again
                    auto cn0 = reinterpret_cast<int32_t&>(triangle.v0.w);
                    auto cn1 = reinterpret_cast<int32_t&>(triangle.v1.w);
                    auto cn2 = reinterpret_cast<int32_t&>(triangle.v2.w);

                    auto n0 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn0));
                    auto n1 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn1));
                    auto n2 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn2));

                    // Compute necessary information
                    auto P = (vec3(triangle.v0) + vec3(triangle.v1) + vec3(triangle.v2)) / 3.0f;
                    auto N = (n0 + n1 + n2) / 3.0f;

                    auto a = glm::distance(vec3(triangle.v1), vec3(triangle.v0));
                    auto b = glm::distance(vec3(triangle.v2), vec3(triangle.v0));
                    auto c = glm::distance(vec3(triangle.v1), vec3(triangle.v2));
                    auto p = 0.5f * (a + b + c);
                    auto area = glm::sqrt(p * (p - a) * (p - b) * (p - c));

                    auto weight = area * brightness;

                    uint32_t data = 0;
                    data |= (1 << 28u); // Type TRIANGLE_LIGHT (see RayTracingHelper.cpp)
                    data |= uint32_t(i);
                    auto cd = reinterpret_cast<float&>(data);

                    GPULight light;
                    light.P = vec4(P, 1.0f);
                    light.N = vec4(N, 0.0f);
                    light.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(radiance), 0.0f);
                    light.data = vec4(cd, weight, area, 0.0f);

                    meshInfo.triangleLights.push_back(light);
                }
            }

        }

        void RTData::UpdateTriangleLights() {

            triangleLights.clear();

            for (auto& [meshIdx, meshInfo] : meshInfos) {

                for (auto& light : meshInfo.triangleLights) {

                    for (size_t i = 0; i < meshInfo.instanceIndices.size(); i++) {

                        auto instanceIdx = meshInfo.instanceIndices[i];
                        auto& matrix = meshInfo.matrices[i];

                        vec3 P = matrix * vec4(vec3(light.P), 1.0f);
                        vec3 N = matrix * vec4(vec3(light.N), 0.0f);

                        auto transformedLight = light;

                        transformedLight.data.w = reinterpret_cast<float&>(instanceIdx);

                        transformedLight.P = vec4(P, light.P.w);
                        transformedLight.N = vec4(N, light.N.w);

                        triangleLights.push_back(transformedLight);

                    }

                }

            }

        }

    }

}