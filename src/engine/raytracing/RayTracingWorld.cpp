#include "RayTracingWorld.h"
#include "scene/Scene.h"

#include "mesh/MeshData.h"
#include "volume/BVH.h"
#include "graphics/ASBuilder.h"
#include "common/ColorConverter.h"

#include <unordered_map>
#include <set>

#include <glm/gtx/norm.hpp>

namespace Atlas {

    namespace RayTracing {

        RayTracingWorld::RayTracingWorld() {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            hardwareRayTracing = device->support.hardwareRayTracing;

            auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HostAccessBit |
                Buffer::BufferUsageBits::HighPriorityMemoryBit | Buffer::BufferUsageBits::MultiBufferedBit;

            materialBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUMaterial));
            bvhInstanceBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHInstance));
            tlasNodeBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHNode));
            lastMatricesBuffer = Buffer::Buffer(bufferUsage, sizeof(mat4x3));

        }

        void RayTracingWorld::Update(Scene::Subset<MeshComponent, TransformComponent> subset, bool updateTriangleLights) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            if (!device->swapChain->isComplete) return;
            if (!subset.Any()) return;

            auto renderState = &scene->renderState;

            blases.clear();

            auto meshes = scene->GetMeshes();
            int32_t meshCount = 0;

            JobSystem::Wait(renderState->bindlessMeshMapUpdateJob);

            std::swap(prevMeshInfos, meshInfos);
            meshInfos.clear();

            for (auto& mesh : meshes) {
                // Only need to check for this, since that means that the BVH was built and the mesh is loaded
                if (!renderState->meshIdToBindlessIdx.contains(mesh.GetID()))
                    continue;

                if (!prevMeshInfos.contains(mesh.GetID())) {
                    meshInfos[mesh.GetID()] = {};
                    BuildTriangleLightsForMesh(mesh);
                }
                else {
                    meshInfos[mesh.GetID()] = prevMeshInfos[mesh.GetID()];
                }

                auto &meshInfo = meshInfos[mesh.GetID()];
                meshInfo.offset = int32_t(renderState->meshIdToBindlessIdx[mesh.GetID()]);
                meshInfo.cullingDistanceSqr = mesh->rayTraceDistanceCulling * mesh->rayTraceDistanceCulling;

                // Some extra path for hardware raytracing, don't want to do work twice
                if (hardwareRayTracing) {
                    if (mesh->needsBvhRefresh && mesh->blas->isDynamic) {
                        blases.push_back(mesh->blas);
                        mesh->needsBvhRefresh = false;
                    }

                    meshInfo.blas = mesh->blas;
                    meshInfo.idx = meshCount++;
                }
            }

            if (hardwareRayTracing) {              
                Graphics::ASBuilder asBuilder;
                if (!blases.empty()) {
                    auto commandList = device->GetCommandList();
                    commandList->BeginCommands();
                    asBuilder.BuildBLAS(blases, commandList);
                    commandList->EndCommands();
                    device->SubmitCommandList(commandList);
                }
            }

            for (auto& [_, meshInfo] : meshInfos) {
                meshInfo.instanceIndices.clear();
                meshInfo.matrices.clear();
            }

            hardwareInstances.clear();
            gpuBvhInstances.clear();
            actorAABBs.clear();
            lastMatrices.clear();

            JobSystem::Wait(renderState->bindlessTextureMapUpdateJob);

            UpdateMaterials();

            JobSystem::Wait(renderState->mainCameraSignal, JobPriority::High);

            vec3 cameraLocation;
            auto hasCamera = scene->HasMainCamera();
            if (hasCamera) {
                auto& camera = scene->GetMainCamera();
                cameraLocation = camera.GetLocation();
            }

            for (auto entity : subset) {
                const auto& [meshComponent, transformComponent] = subset.Get(entity);

                if (!renderState->meshIdToBindlessIdx.contains(meshComponent.mesh.GetID()))
                    continue;

                auto &meshInfo = meshInfos[meshComponent.mesh.GetID()];
                auto distSqd = glm::distance2(
                    vec3(transformComponent.globalMatrix[3]),
                    cameraLocation);
                if (hasCamera && distSqd > meshInfo.cullingDistanceSqr)
                    continue;
                if (hardwareRayTracing && !meshComponent.mesh->blas->isBuilt || meshComponent.mesh->needsBvhRefresh)
                    continue;

                actorAABBs.push_back(meshComponent.aabb);
                auto inverseMatrix = mat3x4(glm::transpose(transformComponent.inverseGlobalMatrix));

                uint32_t mask = InstanceCullMasks::MaskAll;
                mask |= meshComponent.mesh->castShadow ? InstanceCullMasks::MaskShadow : 0;

                GPUBVHInstance gpuBvhInstance = {
                    .inverseMatrix = inverseMatrix,
                    .meshOffset = meshInfo.offset,
                    .materialOffset = meshInfo.materialOffset,
                    .mask = mask
                };                

                meshInfo.matrices.emplace_back(transformComponent.globalMatrix);
                meshInfo.instanceIndices.push_back(uint32_t(gpuBvhInstances.size()));
                gpuBvhInstances.push_back(gpuBvhInstance);

                if (includeObjectHistory)
                    lastMatrices.emplace_back(glm::transpose(transformComponent.lastGlobalMatrix));

                // Some extra path for hardware raytracing, don't want to do work twice
                if (hardwareRayTracing) {
                    VkAccelerationStructureInstanceKHR inst = {};
                    VkTransformMatrixKHR transform;

                    auto transposed = glm::transpose(transformComponent.globalMatrix);
                    std::memcpy(&transform, &transposed, sizeof(VkTransformMatrixKHR));

                    inst.transform = transform;
                    inst.instanceCustomIndex = meshInfo.offset;
                    inst.accelerationStructureReference = meshComponent.mesh->blas->bufferDeviceAddress;
                    inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                    inst.mask = mask;
                    inst.instanceShaderBindingTableRecordOffset = 0;
                    hardwareInstances.push_back(inst);
                }
            }

            if (gpuBvhInstances.empty()) {
                // Need to reset the TLAS in this case, since it might contain invalid memory addresses
                tlas = nullptr;
                return;
            }                

            if (hardwareRayTracing) {
                UpdateForHardwareRayTracing(subset, gpuBvhInstances.size());
            }
            else {
                UpdateForSoftwareRayTracing(gpuBvhInstances, lastMatrices, actorAABBs);
            }

            if (updateTriangleLights)
                UpdateTriangleLights();

            if (bvhInstanceBuffer.GetElementCount() < gpuBvhInstances.size()) {
                bvhInstanceBuffer.SetSize(gpuBvhInstances.size());
            }

            if (lastMatricesBuffer.GetElementCount() < lastMatrices.size() && includeObjectHistory)
                lastMatricesBuffer.SetSize(lastMatrices.size());
           
            bvhInstanceBuffer.SetData(gpuBvhInstances.data(), 0, gpuBvhInstances.size());

            if (includeObjectHistory)
                lastMatricesBuffer.SetData(lastMatrices.data(), 0, lastMatrices.size());

        }

        void RayTracingWorld::UpdateMaterials() {
            
            UpdateMaterials(materials);

        }

        void RayTracingWorld::UpdateMaterials(std::vector<GPUMaterial>& materials) {

            std::lock_guard lock(mutex);

            auto sceneState = &scene->renderState;

            auto meshes = scene->GetMeshes();
            materials.clear();

            for (auto& mesh : meshes) {
                if (!meshInfos.contains(mesh.GetID()) || !mesh.IsLoaded())
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

                    if (material.IsLoaded()) {
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
                        gpuMaterial.cullBackFaces = mesh->cullBackFaces ? 1 : 0;
                        gpuMaterial.useVertexColors = material->vertexColors ? 1 : 0;

                        if (material->HasBaseColorMap()) {
                            gpuMaterial.baseColorTexture = sceneState->textureToBindlessIdx[material->baseColorMap.Get()];
                        }

                        if (material->HasOpacityMap()) {
                            gpuMaterial.opacityTexture = sceneState->textureToBindlessIdx[material->opacityMap.Get()];
                        }

                        if (material->HasNormalMap()) {
                            gpuMaterial.normalTexture = sceneState->textureToBindlessIdx[material->normalMap.Get()];
                        }

                        if (material->HasRoughnessMap()) {
                            gpuMaterial.roughnessTexture = sceneState->textureToBindlessIdx[material->roughnessMap.Get()];
                        }

                        if (material->HasMetalnessMap()) {
                            gpuMaterial.metalnessTexture = sceneState->textureToBindlessIdx[material->metalnessMap.Get()];
                        }

                        if (material->HasAoMap()) {
                            gpuMaterial.aoTexture = sceneState->textureToBindlessIdx[material->aoMap.Get()];
                        }

                        if (material->HasEmissiveMap()) {
                            gpuMaterial.emissiveTexture = sceneState->textureToBindlessIdx[material->emissiveMap.Get()];
                        }
                    }

                    materials.push_back(gpuMaterial);
                }
            }

            if (materials.empty())
                return;

            if (materialBuffer.GetElementCount() < materials.size()) {
                materialBuffer.SetSize(materials.size(), materials.data());
            }
            else {
                materialBuffer.SetData(materials.data(), 0, materials.size());
            }

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
                if (includeObjectHistory)
                    orderedLastMatrices[i] = lastMatrices[bvh.refs[i].idx];
                orderedGpuBvhInstances[i] = gpuBvhInstances[bvh.refs[i].idx];
                orderedGpuBvhInstances[i].nextInstance = ref.endOfNode ? -1 : int32_t(i) + 1;
            }

            if (tlasNodeBuffer.GetElementCount() < gpuBvhNodes.size()) {
                tlasNodeBuffer.SetSize(gpuBvhNodes.size());
            }

            tlasNodeBuffer.SetData(gpuBvhNodes.data(), 0, gpuBvhNodes.size());

            gpuBvhInstances = orderedGpuBvhInstances;
            if (includeObjectHistory)
                lastMatrices = orderedLastMatrices;

        }

        void RayTracingWorld::UpdateForHardwareRayTracing(Scene::Subset<MeshComponent,
            TransformComponent>& entitySubset, size_t instanceCount) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            Graphics::ASBuilder asBuilder;
            auto tlasDesc = Graphics::TLASDesc();
            tlas = device->CreateTLAS(tlasDesc);

            asBuilder.BuildTLAS(tlas, hardwareInstances);

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
                    light.N = vec4(N, area);
                    light.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(radiance) * material->emissiveIntensity, 0.0f);
                    light.data = vec4(cd, weight, 0.0, 0.0f);

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