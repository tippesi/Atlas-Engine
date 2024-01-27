#include "RayTracingWorld.h"
#include "scene/Scene.h"

#include "mesh/MeshData.h"
#include "volume/BVH.h"
#include "graphics/ASBuilder.h"
#include "common/ColorConverter.h"

#include <unordered_map>
#include <set>

namespace Atlas {

    namespace RayTracing {

        RayTracingWorld::RayTracingWorld() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            hardwareRayTracing = device->support.hardwareRayTracing;

            auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit |
               Buffer::BufferUsageBits::MultiBufferedBit | Buffer::BufferUsageBits::HostAccessBit;

            materialBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUMaterial));
            bvhInstanceBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHInstance));
            tlasNodeBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHNode));
            lastMatricesBuffer = Buffer::Buffer(bufferUsage, sizeof(mat4x3));

        }

        void RayTracingWorld::Update(bool updateTriangleLights) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            if (!device->swapChain->isComplete) return;

            auto subset = scene->GetSubset<MeshComponent, TransformComponent>();
            if (!subset.Any()) return;

            auto meshes = scene->GetMeshes();
            for (auto& mesh : meshes) {
                if (!mesh.IsLoaded() || !mesh->IsBVHBuilt() || 
                    !scene->meshIdToBindlessIdx.contains(mesh.GetID()))
                    continue;

                if (!meshInfos.contains(mesh.GetID())) {
                    meshInfos[mesh.GetID()] = {};
                    BuildTriangleLightsForMesh(mesh);
                }

                auto &meshInfo = meshInfos[mesh.GetID()];
                meshInfo.offset = int32_t(scene->meshIdToBindlessIdx[mesh.GetID()]);
            }

            std::vector<GPUBVHInstance> gpuBvhInstances;
            std::vector<Volume::AABB> actorAABBs;
            std::vector<mat3x4> lastMatrices;

            for (auto& [_, meshInfo] : meshInfos) {
                meshInfo.instanceIndices.clear();
                meshInfo.matrices.clear();
            }

            UpdateMaterials();

            for (auto entity : subset) {

                const auto& [meshComponent, transformComponent] = subset.Get(entity);

                if (!meshInfos.contains(meshComponent.mesh.GetID()))
                    continue;

                actorAABBs.push_back(meshComponent.aabb);
                auto &meshInfo = meshInfos[meshComponent.mesh.GetID()];

                auto inverseMatrix = mat3x4(glm::transpose(transformComponent.inverseGlobalMatrix));

                GPUBVHInstance gpuBvhInstance = {
                    .inverseMatrix = inverseMatrix,
                    .meshOffset = meshInfo.offset,
                    .materialOffset = meshInfo.materialOffset
                };

                meshInfo.matrices.emplace_back(transformComponent.globalMatrix);
                meshInfo.instanceIndices.push_back(uint32_t(gpuBvhInstances.size()));
                gpuBvhInstances.push_back(gpuBvhInstance);
                lastMatrices.emplace_back(glm::transpose(transformComponent.lastGlobalMatrix));
            }

            if (gpuBvhInstances.empty())
                return;

            if (hardwareRayTracing) {
                UpdateForHardwareRayTracing(subset);
            }
            else {
                UpdateForSoftwareRayTracing(gpuBvhInstances, lastMatrices, actorAABBs);
            }

            if (updateTriangleLights)
                UpdateTriangleLights();

            if (bvhInstanceBuffer.GetElementCount() < gpuBvhInstances.size()) {
                bvhInstanceBuffer.SetSize(gpuBvhInstances.size());
                lastMatricesBuffer.SetSize(lastMatrices.size());
            }
           
            bvhInstanceBuffer.SetData(gpuBvhInstances.data(), 0, gpuBvhInstances.size());
            lastMatricesBuffer.SetData(lastMatrices.data(), 0, lastMatrices.size());

        }

        void RayTracingWorld::UpdateMaterials() {

            std::vector<GPUMaterial> materials;
            UpdateMaterials(materials);

        }

        void RayTracingWorld::UpdateMaterials(std::vector<GPUMaterial>& materials) {

            std::lock_guard lock(mutex);

            auto meshes = scene->GetMeshes();
            materials.clear();

            for (auto& mesh : meshes) {
                if (!meshInfos.contains(mesh.GetID()))
                    continue;

                auto& meshInfo = meshInfos[mesh.GetID()];
                meshInfo.materialOffset = int32_t(materials.size());

                int32_t meshMaterialID = 0;

                for (auto& material : mesh->data.materials) {
                    GPUMaterial gpuMaterial;

                    size_t hash = mesh.GetID();
                    HashCombine(hash, meshMaterialID++);

                    // Only is persistent when no materials are reorderd in mesh
                    gpuMaterial.ID = int32_t(hash % 65535);

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

            if (materials.empty())
                return;

            materialBuffer.SetSize(materials.size());
            materialBuffer.SetData(materials.data(), 0, materials.size());

        }

        void RayTracingWorld::Clear() {

            meshInfos.clear();

        }

        bool RayTracingWorld::IsValid() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            return materialBuffer.GetSize() > 0 && device->support.bindless && !meshInfos.empty();

        }

        void RayTracingWorld::UpdateForSoftwareRayTracing(std::vector<GPUBVHInstance>& gpuBvhInstances,
            std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& actorAABBs) {

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
            std::vector<mat3x4> orderedLastMatrices(bvh.refs.size());
            for (size_t i = 0; i < bvh.refs.size(); i++) {
                const auto& ref = bvh.refs[i];
                orderedLastMatrices[i] = lastMatrices[bvh.refs[i].idx];
                orderedGpuBvhInstances[i] = gpuBvhInstances[bvh.refs[i].idx];
                orderedGpuBvhInstances[i].nextInstance = ref.endOfNode ? -1 : int32_t(i) + 1;
            }

            if (tlasNodeBuffer.GetElementCount() < gpuBvhNodes.size()) {
                tlasNodeBuffer.SetSize(gpuBvhNodes.size());
            }

            tlasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());

            gpuBvhInstances = orderedGpuBvhInstances;
            lastMatrices = orderedLastMatrices;

        }

        void RayTracingWorld::UpdateForHardwareRayTracing(Scene::Subset<MeshComponent,
            TransformComponent>& entitySubset) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            Graphics::ASBuilder asBuilder;

            auto meshes = scene->GetMeshes();

            blases.clear();

            int32_t meshCount = 0;
            for (auto& mesh : meshes) {
                if (!meshInfos.contains(mesh.GetID()))
                    continue;

                if (mesh->needsBvhRefresh) {
                    blases.push_back(mesh->blas);
                    mesh->needsBvhRefresh = false;
                }

                auto& meshInfo = meshInfos[mesh.GetID()];
                meshInfo.blas = mesh->blas;
                meshInfo.idx = meshCount++;
            }

            if (!blases.empty())
                asBuilder.BuildBLAS(blases);

            std::vector<VkAccelerationStructureInstanceKHR> instances;

            for (auto entity : entitySubset) {
                const auto& [meshComponent, transformComponent] = entitySubset.Get<MeshComponent,
                    TransformComponent>(entity);

                if (!meshInfos.contains(meshComponent.mesh.GetID()))
                    continue;

                const auto& meshInfo = meshInfos[meshComponent.mesh.GetID()];

                VkAccelerationStructureInstanceKHR inst = {};
                VkTransformMatrixKHR transform;

                auto transposed = glm::transpose(transformComponent.globalMatrix);
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

        void RayTracingWorld::BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh> &mesh) {

            auto& gpuTriangles = mesh->data.gpuTriangles;
            auto& materials = mesh->data.materials;

            auto& meshInfo = meshInfos[mesh.GetID()];
            meshInfo.triangleLights.clear();

            // Triangle lights
            for (size_t i = 0; i < gpuTriangles.size(); i++) {
                auto& triangle = gpuTriangles[i];
                auto idx = reinterpret_cast<int32_t&>(triangle.d0.w);
                auto& material = materials[idx];

                auto radiance = Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor);
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
                    light.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(radiance) * material->emissiveIntensity, 0.0f);
                    light.data = vec4(cd, weight, area, 0.0f);

                    meshInfo.triangleLights.push_back(light);
                }
            }

        }

        void RayTracingWorld::UpdateTriangleLights() {

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