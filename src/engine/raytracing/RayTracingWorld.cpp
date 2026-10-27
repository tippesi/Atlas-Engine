#include "RayTracingWorld.h"
#include "common/MathHelper.h"
#include "scene/Scene.h"

#include "mesh/MeshData.h"
#include "volume/BVH.h"
#include "graphics/ASBuilder.h"
#include "graphics/Profiler.h"
#include "pipeline/PipelineManager.h"
#include "common/ColorConverter.h"
#include "common/Packing.h"

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
            jointMatricesBuffer = Buffer::Buffer(bufferUsage, sizeof(mat3x4));

            std::vector<std::string> skinTrianglesMacros;
            if (hardwareRayTracing)
                skinTrianglesMacros.push_back("HARDWARE_RAYTRACING");

            skinTrianglesPipelineConfig = PipelineConfig("raytracer/skinTriangles.csh", skinTrianglesMacros);
            skinVerticesPipelineConfig = PipelineConfig("raytracer/skinVertices.csh");
            refitBVHPipelineConfig = PipelineConfig("raytracer/refitBVH.csh");

            // The hardware instances are written on the GPU
            if (hardwareRayTracing) {
                auto deviceBufferUsage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit;
                bvhInstanceBuffer = Buffer::Buffer(deviceBufferUsage, sizeof(GPUBVHInstance));
                lastMatricesBuffer = Buffer::Buffer(deviceBufferUsage, sizeof(mat3x4));
            }

            jobContexts.resize(6);

        }

        void RayTracingWorld::Update(Scene::Subset<MeshComponent, TransformComponent> subset, bool updateTriangleLights) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            if (!device->swapChain->isComplete) return;
            if (!subset.Any() && !scene->terrain.IsLoaded()) return;

            auto renderState = &scene->renderState;

            buildBlases.clear();
            refitBlases.clear();

            auto meshes = scene->GetMeshes();
            int32_t meshCount = 0;

            JobSystem::Wait(renderState->bindlessBlasMapUpdateJob);

            std::swap(prevBlasInfos, blasInfos);
            blasInfos.clear();

            for (auto& mesh : meshes) {
                // Only need to check for this, since that means that the BVH was built and the mesh is loaded
                if (!mesh.IsLoaded() || !renderState->blasToBindlessIdx.contains(mesh->blas))
                    continue;

                if (!prevBlasInfos.contains(mesh->blas)) {
                    blasInfos[mesh->blas] = BlasInfo{
                        .mesh = mesh,
                    };
                    BuildTriangleLightsForMesh(mesh);
                }
                else {
                    std::swap(blasInfos[mesh->blas], prevBlasInfos[mesh->blas]);
                }

                auto &blasInfo = blasInfos[mesh->blas];
                blasInfo.offset = int32_t(renderState->blasToBindlessIdx[mesh->blas]);
                blasInfo.cullingDistanceSqr = mesh->rayTraceDistanceCulling * mesh->rayTraceDistanceCulling;

                // Some extra path for hardware raytracing, don't want to do work twice
                if (hardwareRayTracing) {
                    if (mesh->blas->needsBvhRefresh && mesh->blas->blas->isDynamic) {
                        buildBlases.push_back(mesh->blas->blas);
                        mesh->blas->needsBvhRefresh = false;
                    }

                    blasInfo.idx = meshCount++;
                }
            }

            for (const auto node : renderState->terrainLeafNodes) {

                if (!node->cell || !node->cell->IsLoaded() || !renderState->blasToBindlessIdx.contains(node->cell->blas))
                    continue;

                if (!prevBlasInfos.contains(node->cell->blas)) {
                    blasInfos[node->cell->blas] = {};
                }
                else {
                    std::swap(blasInfos[node->cell->blas], prevBlasInfos[node->cell->blas]);
                }

                auto &blasInfo = blasInfos[node->cell->blas];

                blasInfo.node = node;
                blasInfo.offset = int32_t(renderState->blasToBindlessIdx[node->cell->blas]);
                blasInfo.cullingDistanceSqr = 100000000000.0f;
            }

            UpdateSkinnedBlases(meshCount);

            // Skinning has to be done before the dynamic BLASes are built
            if (!skinnedBlases.empty() || hardwareRayTracing && !buildBlases.empty()) {
                auto commandList = device->GetCommandList();
                commandList->BeginCommands();
                SkinBlases(commandList);
                if (hardwareRayTracing) {
                    Graphics::ASBuilder asBuilder;
                    if (!buildBlases.empty())
                        asBuilder.BuildBLAS(buildBlases, commandList);
                    if (!refitBlases.empty())
                        asBuilder.UpdateBLAS(refitBlases, commandList);
                }
                commandList->EndCommands();
                device->SubmitCommandList(commandList);
            }

            JobSystem::Wait(renderState->materialUpdateJob);

            UpdateMaterials();

            vec3 cameraLocation = vec3(0.0f);
            auto hasCamera = scene->HasMainCamera();
            if (hasCamera)
                cameraLocation = scene->GetMainCamera().GetLocation();

            if (hardwareRayTracing)
                UpdateForHardwareRayTracing(cameraLocation, hasCamera);
            else
                UpdateForSoftwareRayTracing(cameraLocation, hasCamera);

            if (updateTriangleLights)
                UpdateTriangleLights();

        }

        void RayTracingWorld::UpdateMaterials() {

            UpdateMaterials(materials);

        }

        void RayTracingWorld::UpdateMaterials(std::vector<GPUMaterial>& materials) {

            std::lock_guard lock(mutex);

            auto sceneState = &scene->renderState;

            size_t materialCount = 0;
            for (auto& [blas, blasInfo] : blasInfos)
                materialCount += blas->materials.size();

            materials.clear();

            std::atomic_int counter = 0;

            materials.resize(materialCount);
            std::unordered_map<Ref<BLAS>, BlasInfo>::iterator iterators[4];
            for (size_t i = 0; i < std::size(iterators); i++)
                iterators[i] = blasInfos.begin();

            JobGroup jobGroup{ "Ray tracing world material update", JobPriority::High};
            JobSystem::ParallelFor(jobGroup, int32_t(blasInfos.size()), int32_t(std::size(iterators)),
                [&](JobData& data, int32_t idx) {

                // First advance
                if (iterators[data.idx] == blasInfos.begin())
                    std::advance(iterators[data.idx], idx);
                else
                    iterators[data.idx]++;

                auto& [blas, blasInfo] = *iterators[data.idx];

                blasInfo.materialOffset = counter.fetch_add(int32_t(blas->materials.size()));

                int32_t meshMaterialID = 0;

                for (auto& material : blas->materials) {
                    GPUMaterial gpuMaterial;

                    size_t hash = 0;
                    HashCombine(hash, blas.get());
                    HashCombine(hash, meshMaterialID++);

                    // Only is persistent when no materials are reorderd in mesh
                    gpuMaterial.ID = int32_t(hash % 65535);

                    if (material.IsLoaded()) {
                        auto& mesh = blasInfo.mesh;

                        gpuMaterial.baseColor = Common::ColorConverter::ConvertSRGBToLinear(material->baseColor);
                        gpuMaterial.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(
                            vec4(Helper::Math::Saturate(Common::ColorConverter::ConvertSRGBToLinear(material->transmissiveColor)), 0.0f));
                        gpuMaterial.emissiveColor = Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor)
                            * material->emissiveIntensity;

                        gpuMaterial.opacity = material->opacity;

                        gpuMaterial.roughness = material->roughness;
                        gpuMaterial.metalness = material->metalness;
                        gpuMaterial.ao = material->ao;

                        gpuMaterial.reflectance = material->reflectance;

                        gpuMaterial.normalScale = material->normalScale;

                        gpuMaterial.tiling = material->tiling;
                        gpuMaterial.terrainTiling = blasInfo.node ? 1.0f / blasInfo.node->sideLength : 1.0f;

                        gpuMaterial.invertUVs = mesh.IsLoaded() ? (mesh->invertUVs ? 1 : 0) : 0;
                        gpuMaterial.twoSided = material->twoSided ? 1 : 0;
                        gpuMaterial.cullBackFaces = mesh.IsLoaded() ? (mesh->cullBackFaces ? 1 : 0) : 0;
                        gpuMaterial.useVertexColors = material->vertexColors ? 1 : 0;

                        if (material->HasBaseColorMap()) {
                            gpuMaterial.baseColorTexture = sceneState->GetBindlessTextureIdx(material->baseColorMap.Get());
                        }

                        if (material->HasOpacityMap()) {
                            gpuMaterial.opacityTexture = sceneState->GetBindlessTextureIdx(material->opacityMap.Get());
                        }

                        if (material->HasNormalMap()) {
                            gpuMaterial.normalTexture = sceneState->GetBindlessTextureIdx(material->normalMap.Get());
                        }

                        if (material->HasRoughnessMap()) {
                            gpuMaterial.roughnessTexture = sceneState->GetBindlessTextureIdx(material->roughnessMap.Get());
                        }

                        if (material->HasMetalnessMap()) {
                            gpuMaterial.metalnessTexture = sceneState->GetBindlessTextureIdx(material->metalnessMap.Get());
                        }

                        if (material->HasAoMap()) {
                            gpuMaterial.aoTexture = sceneState->GetBindlessTextureIdx(material->aoMap.Get());
                        }

                        if (material->HasEmissiveMap()) {
                            gpuMaterial.emissiveTexture = sceneState->GetBindlessTextureIdx(material->emissiveMap.Get());
                        }

                        if (blasInfo.node) {
                            gpuMaterial.terrainNormalTexture = sceneState->GetBindlessTextureIdx(blasInfo.node->cell->normalMap);
                        }
                    }

                    materials[blasInfo.materialOffset + meshMaterialID - 1] = gpuMaterial;
                }
                });
            JobSystem::Wait(jobGroup);


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

            blasInfos.clear();
            skinnedBlases.clear();
            skinVertexBuffers.clear();
            pendingInstances.reset();

        }

        bool RayTracingWorld::IsValid() {

            return materialBuffer.GetSize() > 0 && !blasInfos.empty();

        }

        void RayTracingWorld::UpdateForSoftwareRayTracing(vec3 cameraLocation, bool hasCamera) {

            auto renderState = &scene->renderState;

            auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

            gpuBvhInstances.clear();
            instanceAABBs.clear();
            lastMatrices.clear();

            for (auto& jobContext : jobContexts) {
                jobContext.gpuBvhInstances.clear();
                jobContext.instanceAABBs.clear();
                jobContext.lastMatrices.clear();
            }

            JobGroup jobGroup{ "Ray tracing world mesh update", JobPriority::High};
            JobSystem::ParallelFor(jobGroup, int32_t(meshComponentPool.GetCount()), int32_t(jobContexts.size()),
                [&](JobData& data, int32_t idx) {
                    auto& jobContext = jobContexts[data.idx];

                    auto entity = meshComponentPool[idx];
                    auto& meshComponent = meshComponentPool.GetByIndex(idx);

                    if (!meshComponent.visible)
                        return;
                    if (!meshComponent.mesh.IsLoaded() || !meshComponent.mesh->rayTrace)
                        return;

                    auto blas = meshComponent.mesh->blas;
                    auto skinnedBlas = skinnedBlases.find(entity);
                    if (skinnedBlas != skinnedBlases.end() && renderState->blasToBindlessIdx.contains(skinnedBlas->second.blas))
                        blas = skinnedBlas->second.blas;

                    if (!renderState->blasToBindlessIdx.contains(blas) || blas->needsBvhRefresh)
                        return;

                    auto transformComponent = transformComponentPool.TryGet(entity);
                    if (!transformComponent)
                        return;

                    auto& blasInfo = blasInfos[blas];
                    auto distSqd = glm::distance2(
                        vec3(transformComponent->globalMatrix[3]),
                        cameraLocation);
                    if (hasCamera && distSqd > blasInfo.cullingDistanceSqr)
                        return;

                    jobContext.instanceAABBs.push_back(meshComponent.aabb);
                    auto inverseMatrix = mat3x4(glm::transpose(transformComponent->inverseGlobalMatrix));

                    uint32_t mask = InstanceCullMasks::MaskAll;
                    mask |= meshComponent.mesh->castShadow ? InstanceCullMasks::MaskShadow : 0;

                    GPUBVHInstance gpuBvhInstance = {
                        .inverseMatrix = inverseMatrix,
                        .meshOffset = blasInfo.offset,
                        .materialOffset = blasInfo.materialOffset,
                        .mask = mask
                    };

                    jobContext.gpuBvhInstances.emplace_back(gpuBvhInstance);

                    if (includeObjectHistory)
                        jobContext.lastMatrices.emplace_back(glm::transpose(transformComponent->lastGlobalMatrix));
                });

            // The terrain comes first
            for (const auto node : renderState->terrainLeafNodes) {
                if (!node->cell || !node->cell->IsLoaded() || !renderState->blasToBindlessIdx.contains(node->cell->blas))
                    continue;

                const auto &blasInfo = blasInfos[node->cell->blas];
                if (node->cell->blas->needsBvhRefresh)
                    continue;

                auto nodePosition = vec3(node->location.x, 0.0f, node->location.y) + scene->terrain->translation;
                mat4 globalMatrix = glm::translate(nodePosition);
                mat4 inverseGlobalMatrix = glm::inverse(globalMatrix);

                instanceAABBs.push_back(node->cell->aabb.Translate(nodePosition));
                auto inverseMatrix = mat3x4(glm::transpose(inverseGlobalMatrix));

                uint32_t mask = InstanceCullMasks::MaskAll;
                mask |= InstanceCullMasks::MaskShadow;

                GPUBVHInstance gpuBvhInstance = {
                    .inverseMatrix = inverseMatrix,
                    .meshOffset = blasInfo.offset,
                    .materialOffset = blasInfo.materialOffset,
                    .mask = mask
                };

                gpuBvhInstances.push_back(gpuBvhInstance);

                if (includeObjectHistory)
                    lastMatrices.emplace_back(glm::transpose(globalMatrix));
            }

            JobSystem::Wait(jobGroup);

            for (auto& jobContext : jobContexts) {
                gpuBvhInstances.insert(gpuBvhInstances.end(), jobContext.gpuBvhInstances.begin(), jobContext.gpuBvhInstances.end());
                instanceAABBs.insert(instanceAABBs.end(), jobContext.instanceAABBs.begin(), jobContext.instanceAABBs.end());
                lastMatrices.insert(lastMatrices.end(), jobContext.lastMatrices.begin(), jobContext.lastMatrices.end());
            }

            if (gpuBvhInstances.empty()) {
                // Need to reset the TLAS in this case, since it might contain invalid memory addresses
                tlas = nullptr;
                return;
            }

            BuildSoftwareTLAS(gpuBvhInstances, lastMatrices, instanceAABBs);

            if (bvhInstanceBuffer.GetElementCount() < gpuBvhInstances.size()) {
                bvhInstanceBuffer.SetSize(gpuBvhInstances.size());
            }

            if (lastMatricesBuffer.GetElementCount() < lastMatrices.size() && includeObjectHistory)
                lastMatricesBuffer.SetSize(lastMatrices.size());

            bvhInstanceBuffer.SetData(gpuBvhInstances.data(), 0, gpuBvhInstances.size());

            if (includeObjectHistory)
                lastMatricesBuffer.SetData(lastMatrices.data(), 0, lastMatrices.size());

        }

        void RayTracingWorld::UpdateForHardwareRayTracing(vec3 cameraLocation, bool hasCamera) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            auto renderState = &scene->renderState;
            const auto& gpuScene = renderState->gpuScene;

            auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

            auto isUsable = [&](const Ref<BLAS>& blas) {
                return blas && blasInfos.contains(blas) && blas->blas->isBuilt && !blas->needsBvhRefresh;
            };

            auto getMask = [](bool castShadow) {
                return uint32_t(InstanceCullMasks::MaskAll) | (castShadow ? uint32_t(InstanceCullMasks::MaskShadow) : 0u);
            };

            // The instances of the GPU scene reference their mesh by slot
            size_t gpuInstanceCount = 0;
            rtMeshes.resize(gpuScene.slots.size());
            for (size_t i = 0; i < gpuScene.slots.size(); i++) {
                const auto& slot = gpuScene.slots[i];

                GPURTMesh rtMesh;
                if (slot.drawable && slot.mesh->rayTrace && isUsable(slot.mesh->blas)) {
                    const auto& blas = slot.mesh->blas;
                    const auto& blasInfo = blasInfos[blas];
                    rtMesh = GPURTMesh {
                        .blasOffset = uint32_t(blasInfo.offset),
                        .materialOffset = blasInfo.materialOffset,
                        .mask = getMask(slot.mesh->castShadow),
                        .flags = rtMeshUsable,
                        .cullingDistanceSqr = blasInfo.cullingDistanceSqr,
                        .blasAddress = blas->blas->deviceAddress
                    };
                    gpuInstanceCount += slot.entities.size();
                }
                rtMeshes[i] = rtMesh;
            }

            // The terrain and skinned meshes are written on the host
            hardwareInstances.clear();
            gpuBvhInstances.clear();
            lastMatrices.clear();

            auto addInstance = [&](const Ref<BLAS>& blas, const mat4& globalMatrix, const mat4& lastGlobalMatrix, uint32_t mask) {
                const auto& blasInfo = blasInfos[blas];

                VkAccelerationStructureInstanceKHR instance = {};
                auto transposed = glm::transpose(globalMatrix);
                std::memcpy(&instance.transform, &transposed, sizeof(VkTransformMatrixKHR));
                instance.instanceCustomIndex = blasInfo.offset;
                instance.accelerationStructureReference = blas->blas->deviceAddress;
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                instance.mask = mask;
                hardwareInstances.push_back(instance);

                gpuBvhInstances.push_back(GPUBVHInstance {
                    .inverseMatrix = mat3x4(glm::transpose(glm::inverse(globalMatrix))),
                    .meshOffset = blasInfo.offset,
                    .materialOffset = blasInfo.materialOffset,
                    .mask = mask
                    });

                if (includeObjectHistory)
                    lastMatrices.push_back(mat3x4(glm::transpose(lastGlobalMatrix)));
            };

            for (const auto node : renderState->terrainLeafNodes) {
                if (!node->cell || !node->cell->IsLoaded() || !isUsable(node->cell->blas))
                    continue;

                auto nodePosition = vec3(node->location.x, 0.0f, node->location.y) + scene->terrain->translation;
                auto globalMatrix = glm::translate(nodePosition);
                addInstance(node->cell->blas, globalMatrix, globalMatrix, getMask(true));
            }

            for (const auto& slot : gpuScene.slots) {
                if (!slot.skinned || !slot.mesh->rayTrace)
                    continue;

                for (auto entity : slot.entities) {
                    const auto& meshComponent = meshComponentPool.Get(entity);
                    auto transformComponent = transformComponentPool.TryGet(entity);
                    if (!meshComponent.visible || !transformComponent)
                        continue;

                    // Entities further away are traced in the rest pose of their mesh
                    auto blas = slot.mesh->blas;
                    auto skinnedBlas = skinnedBlases.find(entity);
                    if (skinnedBlas != skinnedBlases.end() && isUsable(skinnedBlas->second.blas))
                        blas = skinnedBlas->second.blas;
                    if (!isUsable(blas))
                        continue;

                    auto distanceSqr = glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation);
                    if (hasCamera && distanceSqr > blasInfos[blas].cullingDistanceSqr)
                        continue;

                    addInstance(blas, transformComponent->globalMatrix, transformComponent->lastGlobalMatrix,
                        getMask(slot.mesh->castShadow));
                }
            }

            auto hostInstanceCount = uint32_t(hardwareInstances.size());
            auto maxInstanceCount = hostInstanceCount + uint32_t(gpuInstanceCount);
            if (maxInstanceCount == 0) {
                // Need to reset the TLAS in this case, since it might contain invalid memory addresses
                tlas = nullptr;
                pendingInstances.reset();
                return;
            }

            if (!instanceCountReadback) {
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .domain = Graphics::BufferDomain::Host,
                    .hostAccess = Graphics::BufferHostAccess::Random,
                    .size = sizeof(uint32_t)
                };
                instanceCountReadback = device->CreateMultiBuffer(desc);
            }

            // The acceleration structure is built for an upper bound of the instances. The readback is from the last
            // frame which used the buffers of this one, its instances might have been dropped for a frame if the count
            // went up a lot.
            auto instanceBound = std::min(maxInstanceCount, hostInstanceCount + initialGPUInstanceBound);
            if (readbackFrameCount >= FRAME_DATA_COUNT) {
                auto lastInstanceCount = *static_cast<uint32_t*>(instanceCountReadback->Map());
                instanceCountReadback->Unmap();
                instanceBound = std::clamp(lastInstanceCount + lastInstanceCount / 4 + instanceBoundMargin,
                    hostInstanceCount, maxInstanceCount);
            }
            readbackFrameCount++;

            // The scene can be updated several times per frame, the uploads must not overwrite each other
            auto rtMeshBuffer = uploadBufferPool.Upload(rtMeshes.data(), rtMeshes.size() * sizeof(GPURTMesh));
            auto hostInstanceBuffer = uploadBufferPool.Upload(hardwareInstances.data(),
                hardwareInstances.size() * sizeof(VkAccelerationStructureInstanceKHR));
            auto hostBvhInstanceBuffer = uploadBufferPool.Upload(gpuBvhInstances.data(), gpuBvhInstances.size() * sizeof(GPUBVHInstance));
            auto hostLastMatricesBuffer = includeObjectHistory ?
                uploadBufferPool.Upload(lastMatrices.data(), lastMatrices.size() * sizeof(mat3x4)) : nullptr;

            auto tlasInstanceSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceBound;
            if (!tlasInstanceBuffer || tlasInstanceBuffer->size < tlasInstanceSize) {
                // Some headroom, the instance count changes with the camera
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .domain = Graphics::BufferDomain::Device,
                    .size = tlasInstanceSize + tlasInstanceSize / 4
                };
                tlasInstanceBuffer = device->CreateBuffer(desc);
            }

            if (bvhInstanceBuffer.GetElementCount() < instanceBound)
                bvhInstanceBuffer.SetSize(instanceBound + instanceBound / 4);
            if (includeObjectHistory && lastMatricesBuffer.GetElementCount() < instanceBound)
                lastMatricesBuffer.SetSize(instanceBound + instanceBound / 4);

            if (!instanceCounterBuffer) {
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .domain = Graphics::BufferDomain::Device,
                    .size = sizeof(uint32_t)
                };
                instanceCounterBuffer = device->CreateBuffer(desc);
            }

            // The GPU scene writes its instances while culling, after that the acceleration structure is built
            pendingInstances = PendingInstances {
                .rtMeshBuffer = rtMeshBuffer,
                .hostInstanceBuffer = hostInstanceBuffer,
                .hostBvhInstanceBuffer = hostBvhInstanceBuffer,
                .hostLastMatricesBuffer = hostLastMatricesBuffer,
                .hostInstanceCount = hostInstanceCount,
                .gpuInstanceCount = uint32_t(gpuInstanceCount),
                .instanceBound = instanceBound,
                .includeObjectHistory = includeObjectHistory,
                .frameIndex = device->GetFrameIndex()
            };

            auto tlasDesc = Graphics::TLASDesc{
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
            };
            tlas = device->CreateTLAS(tlasDesc);

        }

        const RayTracingWorld::PendingInstances* RayTracingWorld::GetPendingInstances() const {

            // Updates of frames which weren't rendered are dropped
            auto frameIndex = Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex();
            if (!pendingInstances.has_value() || pendingInstances->frameIndex != frameIndex)
                return nullptr;

            return &pendingInstances.value();

        }

        void RayTracingWorld::PrepareInstances(Graphics::CommandList* commandList) {

            AE_ASSERT(pendingInstances.has_value() && "No instances were prepared by the update");
            if (!pendingInstances.has_value()) return;

            const auto& pending = *pendingInstances;

            // The previous frame might still read the instances
            commandList->MemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            // Instances after the written ones stay zero, without an acceleration structure they are inactive
            uint32_t zero = 0;
            commandList->FillBuffer(tlasInstanceBuffer, &zero);
            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            if (pending.hostInstanceCount > 0) {
                commandList->CopyBuffer(pending.hostInstanceBuffer, tlasInstanceBuffer,
                    VkBufferCopy{ .size = pending.hostInstanceCount * sizeof(VkAccelerationStructureInstanceKHR) });
                commandList->CopyBuffer(pending.hostBvhInstanceBuffer, bvhInstanceBuffer.Get(),
                    VkBufferCopy{ .size = pending.hostInstanceCount * sizeof(GPUBVHInstance) });
                if (pending.includeObjectHistory)
                    commandList->CopyBuffer(pending.hostLastMatricesBuffer, lastMatricesBuffer.Get(),
                        VkBufferCopy{ .size = pending.hostInstanceCount * sizeof(mat3x4) });
            }
            auto hostInstanceCount = pending.hostInstanceCount;
            commandList->FillBuffer(instanceCounterBuffer, &hostInstanceCount);

        }

        void RayTracingWorld::BindInstanceOutputs(Graphics::CommandList* commandList) {

            AE_ASSERT(pendingInstances.has_value() && "No instances were prepared by the update");
            if (!pendingInstances.has_value()) return;

            const auto& pending = *pendingInstances;

            commandList->BindBuffer(pending.rtMeshBuffer, 3, 7);
            commandList->BindBuffer(instanceCounterBuffer, 3, 8);
            commandList->BindBuffer(tlasInstanceBuffer, 3, 9);
            commandList->BindBuffer(bvhInstanceBuffer.Get(), 3, 10);
            // Any buffer works as the target of the last matrices if they aren't written
            commandList->BindBuffer(pending.includeObjectHistory ? lastMatricesBuffer.Get() : bvhInstanceBuffer.Get(), 3, 11);

        }

        void RayTracingWorld::FinishInstances(Graphics::CommandList* commandList) {

            AE_ASSERT(pendingInstances.has_value() && "No instances were prepared by the update");
            if (!pendingInstances.has_value()) return;

            const auto& pending = *pendingInstances;

            Graphics::Profiler::BeginQuery("TLAS update");

            // The written and copied instances are read by the build and the readback
            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            commandList->CopyBuffer(instanceCounterBuffer, instanceCountReadback, VkBufferCopy{ .size = sizeof(uint32_t) });
            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            tlasBuilder.BuildTLAS(tlas, tlasInstanceBuffer, pending.instanceBound, commandList);

            // Ray traced passes of this frame follow in the same command list
            commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            Graphics::Profiler::EndQuery();

            pendingInstances.reset();

        }

        void RayTracingWorld::BuildSoftwareTLAS(std::vector<GPUBVHInstance>& gpuBvhInstances,
            std::vector<mat3x4>& lastMatrices, std::vector<Volume::AABB>& instanceAABBs) {

            auto bvh = Volume::BVH(instanceAABBs);

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

        void RayTracingWorld::UpdateSkinnedBlases(int32_t& meshCount) {

            auto renderState = &scene->renderState;

            auto& animationComponentPool = scene->entityManager.GetPool<AnimationComponent>();
            auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();

            for (auto& [entity, skinnedBlas] : skinnedBlases)
                skinnedBlas.used = false;

            jointMatrices.clear();

            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();
            auto hasCamera = scene->HasMainCamera();
            auto cameraLocation = hasCamera ? scene->GetMainCamera().GetLocation() : vec3(0.0f);

            for (size_t i = 0; i < animationComponentPool.GetCount(); i++) {
                auto entity = animationComponentPool[i];
                const auto& animationComponent = animationComponentPool.GetByIndex(i);
                auto meshComponent = meshComponentPool.TryGet(entity);
                auto transformComponent = transformComponentPool.TryGet(entity);

                if (!meshComponent || !transformComponent || !meshComponent->visible || !meshComponent->mesh.IsLoaded() ||
                    !meshComponent->mesh->rayTrace || !meshComponent->mesh->data.IsSkinned() || animationComponent.jointMatrices.empty())
                    continue;

                auto item = skinnedBlases.find(entity);
                // Entities which already have a BLAS keep it a little further out, so it isn't recreated at the border
                auto distance = item != skinnedBlases.end() ? skinnedBlasDistance * 1.25f : skinnedBlasDistance;
                if (hasCamera && glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation) > distance * distance)
                    continue;

                const auto& mesh = meshComponent->mesh;
                // The hardware BLAS needs to be created again when switching between refits and builds
                if (item == skinnedBlases.end() || item->second.mesh.GetID() != mesh.GetID() ||
                    hardwareRayTracing && item->second.blas->blas->allowUpdate != refitSkinnedBlases)
                    item = skinnedBlases.insert_or_assign(entity, CreateSkinnedBlas(mesh)).first;

                auto& skinnedBlas = item->second;
                skinnedBlas.used = true;
                skinnedBlas.jointOffset = uint32_t(jointMatrices.size());
                jointMatrices.insert(jointMatrices.end(), animationComponent.jointMatrices.begin(),
                    animationComponent.jointMatrices.end());

                // The BLAS is used once the bindless buffers are updated in the next frame
                if (!renderState->blasToBindlessIdx.contains(skinnedBlas.blas))
                    continue;

                if (!prevBlasInfos.contains(skinnedBlas.blas))
                    blasInfos[skinnedBlas.blas] = BlasInfo{ .mesh = mesh };
                else
                    std::swap(blasInfos[skinnedBlas.blas], prevBlasInfos[skinnedBlas.blas]);

                auto& blasInfo = blasInfos[skinnedBlas.blas];
                blasInfo.offset = int32_t(renderState->blasToBindlessIdx[skinnedBlas.blas]);
                blasInfo.cullingDistanceSqr = mesh->rayTraceDistanceCulling * mesh->rayTraceDistanceCulling;

                if (hardwareRayTracing) {
                    const auto& graphicsBlas = skinnedBlas.blas->blas;
                    if (graphicsBlas->allowUpdate && graphicsBlas->isBuilt)
                        refitBlases.push_back(graphicsBlas);
                    else
                        buildBlases.push_back(graphicsBlas);
                    skinnedBlas.blas->needsBvhRefresh = false;

                    blasInfo.idx = meshCount++;
                }
            }

            std::erase_if(skinnedBlases, [](const auto& item) { return !item.second.used; });
            std::erase_if(skinVertexBuffers, [&](const auto& item) {
                return std::none_of(skinnedBlases.begin(), skinnedBlases.end(),
                    [&](const auto& skinnedBlas) { return skinnedBlas.second.mesh.GetID() == item.first; });
                });

            if (jointMatrices.empty())
                return;

            if (jointMatricesBuffer.GetElementCount() < jointMatrices.size())
                jointMatricesBuffer.SetSize(jointMatrices.size());
            jointMatricesBuffer.SetData(jointMatrices.data(), 0, jointMatrices.size());

        }

        void RayTracingWorld::SkinBlases(Graphics::CommandList* commandList) {

            if (skinnedBlases.empty())
                return;

            Graphics::Profiler::BeginThread("Skinning", commandList);
            Graphics::Profiler::BeginQuery("Skin BLASes");

            // The buffers might still be read by the previous frame
            commandList->MemoryBarrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            auto pipeline = PipelineManager::GetPipeline(skinTrianglesPipelineConfig);
            commandList->BindPipeline(pipeline);

            jointMatricesBuffer.Bind(commandList, 3, 1);

            for (auto& [entity, skinnedBlas] : skinnedBlases) {
                auto& blas = skinnedBlas.blas;
                auto triangleCount = uint32_t(blas->triangleIndices.size());

                skinVertexBuffers[skinnedBlas.mesh.GetID()].Bind(commandList, 3, 0);
                skinnedBlas.triangleVertexBuffer.Bind(commandList, 3, 2);
                blas->triangleBuffer.Bind(commandList, 3, 3);
                if (!hardwareRayTracing)
                    blas->bvhTriangleBuffer.Bind(commandList, 3, 4);

                auto constants = SkinPushConstants {
                    .elementCount = triangleCount,
                    .jointOffset = skinnedBlas.jointOffset
                };
                commandList->PushConstants("constants", &constants);

                commandList->Dispatch((triangleCount + 63) / 64, 1, 1);
            }

            if (hardwareRayTracing) {
                pipeline = PipelineManager::GetPipeline(skinVerticesPipelineConfig);
                commandList->BindPipeline(pipeline);

                for (auto& [entity, skinnedBlas] : skinnedBlases) {
                    auto vertexCount = uint32_t(skinnedBlas.vertexBuffer.elementCount);

                    skinVertexBuffers[skinnedBlas.mesh.GetID()].Bind(commandList, 3, 0);
                    commandList->BindBuffer(skinnedBlas.vertexBuffer.buffer, 3, 2);

                    auto constants = SkinPushConstants {
                        .elementCount = vertexCount,
                        .jointOffset = skinnedBlas.jointOffset
                    };
                    commandList->PushConstants("constants", &constants);

                    commandList->Dispatch((vertexCount + 63) / 64, 1, 1);
                }

                commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
            }
            else {
                commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                pipeline = PipelineManager::GetPipeline(refitBVHPipelineConfig);
                commandList->BindPipeline(pipeline);

                size_t levelCount = 0;
                for (const auto& [entity, skinnedBlas] : skinnedBlases)
                    levelCount = std::max(levelCount, skinnedBlas.levelOffsets.size() - 1);

                // The levels of all BLASes are updated together, starting with the deepest one
                for (int32_t level = int32_t(levelCount) - 1; level >= 0; level--) {
                    for (auto& [entity, skinnedBlas] : skinnedBlases) {
                        const auto& levelOffsets = skinnedBlas.levelOffsets;
                        if (level + 1 >= int32_t(levelOffsets.size()))
                            continue;

                        auto& blas = skinnedBlas.blas;
                        blas->blasNodeBuffer.Bind(commandList, 3, 0);
                        blas->bvhTriangleBuffer.Bind(commandList, 3, 1);
                        skinnedBlas.levelNodeBuffer.Bind(commandList, 3, 2);
                        skinnedBlas.nodeBoundsBuffer.Bind(commandList, 3, 3);

                        auto constants = RefitPushConstants {
                            .nodeOffset = levelOffsets[level],
                            .nodeCount = levelOffsets[level + 1] - levelOffsets[level]
                        };
                        commandList->PushConstants("constants", &constants);

                        commandList->Dispatch((constants.nodeCount + 63) / 64, 1, 1);
                    }

                    commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                }
            }

            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndThread();

        }

        RayTracingWorld::SkinnedBlas RayTracingWorld::CreateSkinnedBlas(const ResourceHandle<Mesh::Mesh>& mesh) {

            auto& data = mesh->data;
            auto vertexCount = size_t(data.GetVertexCount());

            if (!skinVertexBuffers.contains(mesh.GetID())) {
                std::vector<GPUSkinVertex> skinVertices(vertexCount);
                for (size_t i = 0; i < vertexCount; i++) {
                    auto packedNormal = Common::Packing::PackSignedVector3x10_1x2(vec4(vec3(data.normals[i]), 0.0f));
                    auto joints = glm::uvec4(data.joints[i]);

                    skinVertices[i].position = vec4(data.vertices[i], reinterpret_cast<float&>(packedNormal));
                    skinVertices[i].joints01 = joints.x | (joints.y << 16);
                    skinVertices[i].joints23 = joints.z | (joints.w << 16);
                    skinVertices[i].weights = glm::packUnorm4x8(data.weights[i]);
                }

                skinVertexBuffers[mesh.GetID()] = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                    sizeof(GPUSkinVertex), skinVertices.size(), skinVertices.data());
            }

            SkinnedBlas skinnedBlas = {
                .mesh = mesh,
                .blas = CreateRef<BLAS>()
            };

            std::vector<BLAS::Triangle> triangles;
            data.BuildBVHData(triangles);

            if (hardwareRayTracing) {
                std::vector<Graphics::ASGeometryRegion> geometryRegions;
                for (auto& subData : data.subData) {
                    geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                        .indexCount = subData.indicesCount,
                        .indexOffset = subData.indicesOffset,
                        .opaque = !subData.material->HasOpacityMap() && subData.material->opacity == 1.0f
                        });
                }

                skinnedBlas.vertexBuffer = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, vertexCount,
                    data.vertices.Get().data());
                skinnedBlas.blas->isDynamic = true;
                skinnedBlas.blas->allowUpdate = refitSkinnedBlases;
                skinnedBlas.blas->Build(triangles, data.materials, skinnedBlas.vertexBuffer, mesh->indexBuffer,
                    geometryRegions);
            }
            else {
                skinnedBlas.blas->Build(triangles, data.materials);

                // Parents always come before their children
                const auto& nodeParents = skinnedBlas.blas->nodeParents;
                std::vector<int32_t> nodeLevels(nodeParents.size(), 0);
                std::vector<std::vector<int32_t>> levels;
                for (size_t i = 0; i < nodeParents.size(); i++) {
                    auto parent = nodeParents[i];
                    nodeLevels[i] = parent >= 0 ? nodeLevels[parent >> 1] + 1 : 0;
                    if (nodeLevels[i] >= int32_t(levels.size()))
                        levels.resize(nodeLevels[i] + 1);
                    levels[nodeLevels[i]].push_back(int32_t(i));
                }

                std::vector<int32_t> levelNodes;
                for (const auto& level : levels) {
                    skinnedBlas.levelOffsets.push_back(uint32_t(levelNodes.size()));
                    levelNodes.insert(levelNodes.end(), level.begin(), level.end());
                }
                skinnedBlas.levelOffsets.push_back(uint32_t(levelNodes.size()));

                if (!levelNodes.empty()) {
                    skinnedBlas.levelNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                        sizeof(int32_t), levelNodes.size(), levelNodes.data());
                    skinnedBlas.nodeBoundsBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                        2 * sizeof(vec4), levelNodes.size());
                }
            }

            const auto& triangleIndices = skinnedBlas.blas->triangleIndices;
            std::vector<ivec4> triangleVertices(triangleIndices.size());
            for (size_t i = 0; i < triangleIndices.size(); i++) {
                auto idx = size_t(triangleIndices[i]) * 3;
                triangleVertices[i] = ivec4(data.indices[idx], data.indices[idx + 1], data.indices[idx + 2], 0);
            }

            skinnedBlas.triangleVertexBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                sizeof(ivec4), triangleVertices.size(), triangleVertices.data());

            return skinnedBlas;

        }

        void RayTracingWorld::BuildTriangleLightsForMesh(ResourceHandle<Mesh::Mesh> &mesh) {

            auto& gpuTriangles = mesh->blas->gpuTriangles;
            const auto& blasMaterials = mesh->blas->materials;

            auto& blasInfo = blasInfos[mesh->blas];
            blasInfo.triangleLights.clear();

            // Triangle lights
            for (size_t i = 0; i < gpuTriangles.size(); i++) {
                auto& triangle = gpuTriangles[i];
                auto idx = reinterpret_cast<int32_t&>(triangle.d0.w);
                auto& material = blasMaterials[idx];

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

                    blasInfo.triangleLights.push_back(light);
                }
            }

        }

        void RayTracingWorld::UpdateTriangleLights() {

            triangleLights.clear();

            /*
            for (auto& [meshIdx, blasInfo] : blasInfos) {

                for (auto& light : blasInfo.triangleLights) {

                    for (size_t i = 0; i < blasInfo.instanceIndices.size(); i++) {

                        auto instanceIdx = blasInfo.instanceIndices[i];
                        auto& matrix = blasInfo.matrices[i];

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
            */

        }

    }

}
