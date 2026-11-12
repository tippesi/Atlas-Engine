#include "RayTracingWorld.h"
#include "common/MathHelper.h"
#include "scene/Scene.h"

#include "mesh/MeshData.h"
#include "volume/BVH.h"
#include "graphics/ASBuilder.h"
#include "profiler/CPUProfiler.h"
#include "profiler/GPUProfiler.h"
#include "pipeline/PipelineManager.h"
#include "common/ColorConverter.h"
#include "common/Packing.h"
#include "common/Hash.h"
#include "materialgraph/PipelineCache.h"

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
            skinJobBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUSkinJob));
            skinGroupJobBuffer = Buffer::Buffer(bufferUsage, sizeof(uint32_t));

            skinTrianglesPipelineConfig = PipelineConfig("raytracer/skinTriangles.csh");
            refitBVHPipelineConfig = PipelineConfig("raytracer/refitBVH.csh");

            // The hardware instances are written on the GPU
            if (hardwareRayTracing) {
                auto deviceBufferUsage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit;
                bvhInstanceBuffer = Buffer::Buffer(deviceBufferUsage, sizeof(GPUBVHInstance));
                lastMatricesBuffer = Buffer::Buffer(deviceBufferUsage, sizeof(mat3x4));
            }

            jobContexts.resize(6);

        }

        void RayTracingWorld::Update(Scene::Subset<MeshComponent, TransformComponent> subset,
            const std::vector<ECS::Entity>& dirtyEntities, bool updateTriangleLights) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            if (!device->swapChain->isComplete) return;
            if (!subset.Any() && !scene->terrain.IsLoaded()) return;

            auto renderState = &scene->renderState;

            CPUProfiler::Scope profilerScope("Blas infos");
            buildBlases.clear();

            auto meshes = scene->GetMeshes();
            int32_t meshCount = 0;

            JobSystem::Wait(renderState->bindlessBlasMapUpdateJob);

            blasInfoUpdate++;

            for (auto& mesh : meshes) {
                // Only need to check for this, since that means that the BVH was built and the mesh is loaded
                if (!mesh.IsLoaded())
                    continue;
                auto bindlessIdx = renderState->blasToBindlessIdx.find(mesh->blas);
                if (bindlessIdx == renderState->blasToBindlessIdx.end())
                    continue;

                auto [blasInfoIt, inserted] = blasInfos.try_emplace(mesh->blas);
                auto& blasInfo = blasInfoIt->second;
                if (inserted) {
                    blasInfo.mesh = mesh;
                    BuildTriangleLightsForMesh(mesh);
                    blasInfosChanged = true;
                }

                blasInfo.update = blasInfoUpdate;
                blasInfo.offset = int32_t(bindlessIdx->second);
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

                if (!node->cell || !node->cell->IsLoaded())
                    continue;
                auto bindlessIdx = renderState->blasToBindlessIdx.find(node->cell->blas);
                if (bindlessIdx == renderState->blasToBindlessIdx.end())
                    continue;

                auto [blasInfoIt, inserted] = blasInfos.try_emplace(node->cell->blas);
                auto& blasInfo = blasInfoIt->second;
                blasInfosChanged |= inserted;

                blasInfo.update = blasInfoUpdate;
                blasInfo.node = node;
                blasInfo.offset = int32_t(bindlessIdx->second);
                blasInfo.cullingDistanceSqr = 100000000000.0f;
            }

            profilerScope.Next("Skinned blases");
            // The deformation reads the material graph map, which the material update rebuilds
            JobSystem::Wait(renderState->materialUpdateJob);
            if (hardwareRayTracing)
                UpdateDeformedInstances(dirtyEntities);
            else
                UpdateSkinnedBlases();

            std::erase_if(blasInfos, [&](auto& entry) {
                if (entry.second.update == blasInfoUpdate)
                    return false;
                ReleaseHitRecords(entry.second);
                blasInfosChanged = true;
                return true;
                });

            // Skinning has to be done before the dynamic BLASes are built
            // The deformation of hardware ray tracing is recorded with the instances, see RecordDeformation
            if (hardwareRayTracing ? !buildBlases.empty() : !jointMatrices.empty()) {
                auto commandList = device->GetCommandList();
                commandList->BeginCommands();
                if (hardwareRayTracing) {
                    GPUProfiler::BeginThread("Dynamic BLASes", commandList);
                    GPUProfiler::BeginQuery("Build BLASes");
                    blasBuilder.BuildBLAS(buildBlases, commandList);
                    GPUProfiler::EndQuery();
                    GPUProfiler::EndThread();
                }
                else {
                    SkinBlases(commandList);
                }
                commandList->EndCommands();
                device->SubmitCommandList(commandList);
            }

            profilerScope.Next("Materials");
            UpdateMaterials();

            profilerScope.Next("Instances");
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

            struct GraphHit {
                uint32_t hitGroup;
                uint32_t parameterOffset;
            };
            std::unordered_map<void*, GraphHit> graphHits;

            // Hit shaders compile in the background, graphs without compiled ones use their baked values. Graphs
            // without an opacity use the any hit stage of the other materials
            auto isCompiled = [](const std::string& file, VkShaderStageFlagBits stage) {
                return file.empty() || MaterialGraph::PipelineCache::RequestStage(file, stage) == MaterialGraph::PipelineCache::State::Ready;
            };
            std::set<MaterialGraphHitGroup> hitGroups;
            for (const auto& [material, data] : sceneState->materialGraphMap) {
                auto compiled = static_cast<Material*>(material)->graph->GetCompiled();
                if (compiled && !compiled->hitFile.empty() && isCompiled(compiled->hitFile, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) &&
                    isCompiled(compiled->hitOpacityFile, VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
                    hitGroups.insert({ compiled->hitFile, compiled->hitOpacityFile });
            }
            if (!materialGraphHitGroups || !std::equal(materialGraphHitGroups->begin(), materialGraphHitGroups->end(),
                hitGroups.begin(), hitGroups.end()))
                materialGraphHitGroups = CreateRef<const std::vector<MaterialGraphHitGroup>>(hitGroups.begin(), hitGroups.end());
            const auto& graphHitGroups = *materialGraphHitGroups;

            for (const auto& [material, data] : sceneState->materialGraphMap) {
                auto compiled = static_cast<Material*>(material)->graph->GetCompiled();
                if (!compiled)
                    continue;

                // Hit group 0 has no closest hit stage
                auto hitGroup = std::lower_bound(graphHitGroups.begin(), graphHitGroups.end(),
                    MaterialGraphHitGroup{ compiled->hitFile, compiled->hitOpacityFile });
                if (hitGroup != graphHitGroups.end() && hitGroup->closestHitFile == compiled->hitFile)
                    graphHits[material] = { uint32_t(hitGroup - graphHitGroups.begin()) + 1, data.parameterOffset };
            }

            // Graphs without an opacity are opaque, the BLAS geometries of graphs aren't
            std::unordered_set<void*> opaqueGraphs;
            Hash key = 0;
            for (const auto& [material, data] : sceneState->materialGraphMap) {
                auto graphMaterial = static_cast<Material*>(material);
                auto compiled = graphMaterial->graph->GetCompiled();
                if (compiled && compiled->valid && !compiled->hasOpacity && !graphMaterial->opacityMap.IsValid() &&
                    graphMaterial->opacity == 1.0f)
                    opaqueGraphs.insert(material);

                Hash entryKey = 0;
                HashCombine(entryKey, material);
                HashCombine(entryKey, compiled.get());
                key ^= entryKey;
            }
            for (const auto& hitGroup : graphHitGroups) {
                HashCombine(key, hitGroup.closestHitFile);
                HashCombine(key, hitGroup.anyHitFile);
            }

            if (blasInfosChanged || key != hitRecordsKey) {
                hitRecordsKey = key;
                blasInfosChanged = false;

                uint32_t maxGeometryCount = 1;
                for (const auto& [blas, blasInfo] : blasInfos) {
                    if (blasInfo.mesh.IsLoaded())
                        maxGeometryCount = std::max(maxGeometryCount, uint32_t(blasInfo.mesh->data.subData.size()));
                }

                // The records of meshes without graphs cover the largest geometry count, the others move when it grows
                bool hitRecordsChanged = false;
                if (maxGeometryCount > defaultHitRecordCount) {
                    defaultHitRecordCount = maxGeometryCount;
                    hitRecords.assign(defaultHitRecordCount, 0);
                    freeHitRecords.clear();
                    for (auto& [blas, blasInfo] : blasInfos) {
                        blasInfo.hitRecordOffset = 0;
                        blasInfo.hitRecordCount = 0;
                    }
                    hitRecordsChanged = true;
                }

                std::vector<uint32_t> geometryHitGroups;
                for (auto& [blas, blasInfo] : blasInfos) {
                    geometryHitGroups.clear();
                    bool hasGraph = false;
                    bool opaque = true;
                    if (blasInfo.mesh.IsLoaded()) {
                        for (const auto& subData : blasInfo.mesh->data.subData) {
                            const auto& material = subData.material;
                            if (!material.IsLoaded()) {
                                geometryHitGroups.push_back(0);
                                opaque = false;
                                continue;
                            }

                            auto graphHit = graphHits.find(material.Get().get());
                            geometryHitGroups.push_back(graphHit != graphHits.end() ? graphHit->second.hitGroup : 0);
                            hasGraph |= material->HasGraph();
                            opaque &= material->HasGraph() ? opaqueGraphs.contains(material.Get().get()) :
                                !material->opacityMap.IsValid() && material->opacity == 1.0f;
                        }
                    }
                    blasInfo.forceOpaque = hasGraph && opaque;

                    if (std::all_of(geometryHitGroups.begin(), geometryHitGroups.end(), [](uint32_t group) { return group == 0; })) {
                        ReleaseHitRecords(blasInfo);
                        continue;
                    }

                    auto count = uint32_t(geometryHitGroups.size());
                    if (blasInfo.hitRecordCount != count) {
                        ReleaseHitRecords(blasInfo);
                        AllocateHitRecords(blasInfo, count);
                    }

                    auto records = hitRecords.begin() + blasInfo.hitRecordOffset;
                    if (!std::equal(geometryHitGroups.begin(), geometryHitGroups.end(), records)) {
                        std::copy(geometryHitGroups.begin(), geometryHitGroups.end(), records);
                        hitRecordsChanged = true;
                    }
                }
                if (hitRecordsChanged)
                    hitRecordsVersion++;
            }
            if (!hitRecordsSnapshot || hitRecordsSnapshotVersion != hitRecordsVersion) {
                hitRecordsSnapshot = CreateRef<const std::vector<uint32_t>>(hitRecords);
                hitRecordsSnapshotVersion = hitRecordsVersion;
            }

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

                        if (material->baseColorMap.IsLoaded()) {
                            gpuMaterial.baseColorTexture = sceneState->GetBindlessTextureIdx(material->baseColorMap.Get());
                        }

                        if (material->opacityMap.IsLoaded()) {
                            gpuMaterial.opacityTexture = sceneState->GetBindlessTextureIdx(material->opacityMap.Get());
                        }

                        if (material->normalMap.IsLoaded()) {
                            gpuMaterial.normalTexture = sceneState->GetBindlessTextureIdx(material->normalMap.Get());
                        }

                        if (material->roughnessMap.IsLoaded()) {
                            gpuMaterial.roughnessTexture = sceneState->GetBindlessTextureIdx(material->roughnessMap.Get());
                        }

                        if (material->metalnessMap.IsLoaded()) {
                            gpuMaterial.metalnessTexture = sceneState->GetBindlessTextureIdx(material->metalnessMap.Get());
                        }

                        if (material->aoMap.IsLoaded()) {
                            gpuMaterial.aoTexture = sceneState->GetBindlessTextureIdx(material->aoMap.Get());
                        }

                        if (material->emissiveMap.IsLoaded()) {
                            gpuMaterial.emissiveTexture = sceneState->GetBindlessTextureIdx(material->emissiveMap.Get());
                        }

                        if (blasInfo.node) {
                            gpuMaterial.terrainNormalTexture = sceneState->GetBindlessTextureIdx(blasInfo.node->cell->normalMap);
                        }

                        auto graphHit = graphHits.find(material.Get().get());
                        if (graphHit != graphHits.end()) {
                            gpuMaterial.graphHitGroup = graphHit->second.hitGroup;
                            gpuMaterial.graphParameterOffset = graphHit->second.parameterOffset;
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
            deformedInstances.clear();
            deformMeshes.clear();
            pendingInstances.reset();

            hitRecords.clear();
            freeHitRecords.clear();
            defaultHitRecordCount = 0;
            hitRecordsVersion++;
            hitRecordsSnapshot.reset();
            materialGraphHitGroups.reset();
            renderHitRecords.reset();
            renderMaterialGraphHitGroups.reset();
            materialGraphPipelineConfigs.clear();
            shaderBindingTables.clear();

        }

        const Graphics::ShaderBindingTable& RayTracingWorld::GetShaderBindingTable(const Ref<Graphics::Pipeline>& pipeline,
            Graphics::CommandList* commandList) {

            // Tables which weren't requested for a while belong to pipeline variants that aren't used anymore
            shaderBindingTableRequests++;
            std::erase_if(shaderBindingTables, [&](const auto& entry) {
                return entry.lastRequest + 256 < shaderBindingTableRequests;
                });

            auto entry = std::find_if(shaderBindingTables.begin(), shaderBindingTables.end(),
                [&](const auto& entry) { return entry.pipeline == pipeline; });
            if (entry == shaderBindingTables.end())
                entry = shaderBindingTables.insert(shaderBindingTables.end(), { .pipeline = pipeline });

            if (!entry->table.buffer || entry->hitRecords != renderHitRecords) {
                AE_ASSERT(renderHitRecords && "Shader binding tables need prepared instances");
                entry->table = pipeline->CreateShaderBindingTable(renderHitRecords ? *renderHitRecords : std::vector<uint32_t>{},
                    commandList);
                entry->hitRecords = renderHitRecords;
            }
            entry->lastRequest = shaderBindingTableRequests;

            return entry->table;

        }

        PipelineConfig* RayTracingWorld::GetMaterialGraphPipelineConfig(const PipelineConfig& config) {

            if (!renderMaterialGraphHitGroups || renderMaterialGraphHitGroups->empty())
                return nullptr;

            auto key = config.shaderHash;
            HashCombine(key, config.variantHash);
            auto [item, inserted] = materialGraphPipelineConfigs.try_emplace(key);
            if (!inserted)
                return &item->second;

            // Closest hit stage i forms a group with any hit stage i + 1, see Pipeline
            auto shaderConfig = config.shaderConfig;
            for (const auto& hitGroup : *renderMaterialGraphHitGroups) {
                shaderConfig.push_back({ hitGroup.closestHitFile, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });
                shaderConfig.push_back({ hitGroup.anyHitFile.empty() ? "raytracer/opacity.rahit" : hitGroup.anyHitFile,
                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR });
            }

            item->second = PipelineConfig(shaderConfig, config.macros);
            item->second.AddMacro("MATERIAL_GRAPH_HITS");
            return &item->second;

        }

        void RayTracingWorld::AllocateHitRecords(BlasInfo& blasInfo, uint32_t count) {

            // First fit, the rest of a larger block stays free
            auto freeBlock = std::find_if(freeHitRecords.begin(), freeHitRecords.end(),
                [&](const auto& block) { return block.second >= count; });
            if (freeBlock != freeHitRecords.end()) {
                blasInfo.hitRecordOffset = freeBlock->first;
                freeBlock->first += count;
                freeBlock->second -= count;
                if (freeBlock->second == 0)
                    freeHitRecords.erase(freeBlock);
            }
            else {
                blasInfo.hitRecordOffset = uint32_t(hitRecords.size());
                hitRecords.resize(hitRecords.size() + count, 0);
            }
            blasInfo.hitRecordCount = count;

        }

        void RayTracingWorld::ReleaseHitRecords(BlasInfo& blasInfo) {

            if (blasInfo.hitRecordCount == 0)
                return;

            std::fill_n(hitRecords.begin() + blasInfo.hitRecordOffset, blasInfo.hitRecordCount, 0u);

            // Free blocks are sorted by offset and merged with their neighbours
            auto block = std::lower_bound(freeHitRecords.begin(), freeHitRecords.end(),
                std::make_pair(blasInfo.hitRecordOffset, 0u));
            block = freeHitRecords.insert(block, { blasInfo.hitRecordOffset, blasInfo.hitRecordCount });
            auto next = std::next(block);
            if (next != freeHitRecords.end() && block->first + block->second == next->first) {
                block->second += next->second;
                freeHitRecords.erase(next);
            }
            if (block != freeHitRecords.begin()) {
                auto previous = std::prev(block);
                if (previous->first + previous->second == block->first) {
                    previous->second += block->second;
                    freeHitRecords.erase(block);
                }
            }

            blasInfo.hitRecordOffset = 0;
            blasInfo.hitRecordCount = 0;
            hitRecordsVersion++;

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
                if (slot.geometry && slot.mesh->rayTrace && isUsable(slot.mesh->blas)) {
                    const auto& blas = slot.mesh->blas;
                    const auto& blasInfo = blasInfos[blas];
                    rtMesh = GPURTMesh {
                        .blasOffset = uint32_t(blasInfo.offset),
                        .materialOffset = blasInfo.materialOffset,
                        .mask = getMask(slot.mesh->castShadow),
                        .flags = rtMeshUsable | (blasInfo.forceOpaque ? rtMeshForceOpaque : 0u),
                        .cullingDistanceSqr = blasInfo.cullingDistanceSqr,
                        .deformDistanceSqr = deformedMeshes.contains(slot.mesh.GetID()) && hasCamera ?
                            deformedBlasDistance * deformedBlasDistance : 0.0f,
                        .blasAddress = blas->blas->deviceAddress,
                        .hitRecordOffset = blasInfo.hitRecordOffset
                    };
                    gpuInstanceCount += slot.entities.size();
                }
                rtMeshes[i] = rtMesh;
            }

            // The terrain and skinned meshes are written on the host
            hardwareInstances.clear();
            gpuBvhInstances.clear();
            lastMatrices.clear();

            // Deformed instances trace their own BLAS, the hits read everything but the normals from the mesh
            auto addInstance = [&](const Ref<BLAS>& blas, const mat4& globalMatrix, const mat4& lastGlobalMatrix, uint32_t mask,
                const DeformedInstance* deformed = nullptr) {
                const auto& blasInfo = blasInfos[blas];

                VkAccelerationStructureInstanceKHR instance = {};
                auto transposed = glm::transpose(globalMatrix);
                std::memcpy(&instance.transform, &transposed, sizeof(VkTransformMatrixKHR));
                instance.instanceCustomIndex = blasInfo.offset;
                instance.instanceShaderBindingTableRecordOffset = blasInfo.hitRecordOffset;
                instance.accelerationStructureReference = deformed ? deformed->blas->deviceAddress : blas->blas->deviceAddress;
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                    (blasInfo.forceOpaque ? VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR : 0);
                instance.mask = mask;
                hardwareInstances.push_back(instance);

                GPUBVHInstance gpuBvhInstance = {
                    .inverseMatrix = mat3x4(glm::transpose(glm::inverse(globalMatrix))),
                    .meshOffset = blasInfo.offset,
                    .materialOffset = blasInfo.materialOffset,
                    .mask = mask
                };
                if (deformed) {
                    const auto& indexBuffer = deformed->mesh->indexBuffer;
                    gpuBvhInstance.deformedVertices = deformed->normalBuffer.Get()->GetDeviceAddress();
                    gpuBvhInstance.meshIndices = indexBuffer.buffer->GetDeviceAddress() | (indexBuffer.elementSize == 2 ? 1 : 0);
                }
                gpuBvhInstances.push_back(gpuBvhInstance);

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

                    const auto& blas = slot.mesh->blas;
                    if (!isUsable(blas))
                        continue;

                    auto distanceSqr = glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation);
                    if (hasCamera && distanceSqr > blasInfos[blas].cullingDistanceSqr)
                        continue;

                    // Entities further away are traced in the rest pose of their mesh
                    auto deformed = deformedInstances.find(entity);
                    auto deformedInstance = deformed != deformedInstances.end() && deformed->second.blas->isBuilt ?
                        &deformed->second : nullptr;
                    addInstance(blas, transformComponent->globalMatrix, transformComponent->lastGlobalMatrix,
                        getMask(slot.mesh->castShadow), deformedInstance);
                }
            }

            // Until its BLAS is built, a deformed instance traces the BLAS of its mesh
            for (const auto& [entity, instance] : deformedInstances) {
                if (!instance.deformedStatic || !instance.active || !isUsable(instance.mesh->blas))
                    continue;

                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!transformComponent)
                    continue;

                const auto& blas = instance.mesh->blas;
                auto distanceSqr = glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation);
                if (hasCamera && distanceSqr > blasInfos[blas].cullingDistanceSqr)
                    continue;

                addInstance(blas, transformComponent->globalMatrix, transformComponent->lastGlobalMatrix,
                    getMask(instance.mesh->castShadow), instance.blas->isBuilt ? &instance : nullptr);
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
                instanceCountReadback->Invalidate();
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

            auto tlasDesc = Graphics::TLASDesc{
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
            };

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
                .frameIndex = device->GetFrameIndex(),
                .tlas = device->CreateTLAS(tlasDesc),
                .deformDispatches = deformDispatches,
                .deformBlases = deformBlases,
                .deformJobBuffer = deformJobUpload,
                .deformGroupJobBuffer = deformGroupJobUpload,
                .jointMatrixBuffer = jointMatrixUpload,
                .hitRecords = hitRecordsSnapshot,
                .materialGraphHitGroups = materialGraphHitGroups
            };

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

            renderHitRecords = pending.hitRecords;
            if (renderMaterialGraphHitGroups != pending.materialGraphHitGroups) {
                renderMaterialGraphHitGroups = pending.materialGraphHitGroups;
                materialGraphPipelineConfigs.clear();
            }

            RecordDeformation(commandList);

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

            auto& pending = *pendingInstances;

            GPUProfiler::BeginQuery("TLAS update");

            // The written and copied instances are read by the build and the readback
            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            commandList->CopyBuffer(instanceCounterBuffer, instanceCountReadback, VkBufferCopy{ .size = sizeof(uint32_t) });
            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            tlasBuilder.BuildTLAS(pending.tlas, tlasInstanceBuffer, pending.instanceBound, commandList);
            tlas = pending.tlas;

            // Ray traced passes of this frame follow in the same command list
            commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            GPUProfiler::EndQuery();

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

        void RayTracingWorld::UpdateSkinnedBlases() {

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
                auto created = item == skinnedBlases.end() || item->second.mesh.GetID() != mesh.GetID();
                if (created)
                    item = skinnedBlases.insert_or_assign(entity, CreateSkinnedBlas(mesh)).first;

                auto& skinnedBlas = item->second;
                skinnedBlas.used = true;
                skinnedBlas.skin = created || animationComponent.IsPoseUpdated();
                if (skinnedBlas.skin) {
                    skinnedBlas.jointOffset = uint32_t(jointMatrices.size());
                    jointMatrices.insert(jointMatrices.end(), animationComponent.jointMatrices.begin(),
                        animationComponent.jointMatrices.end());
                }

                // The BLAS is used once the bindless buffers are updated in the next frame
                auto bindlessIdx = renderState->blasToBindlessIdx.find(skinnedBlas.blas);
                if (bindlessIdx == renderState->blasToBindlessIdx.end())
                    continue;

                auto [blasInfoIt, inserted] = blasInfos.try_emplace(skinnedBlas.blas);
                auto& blasInfo = blasInfoIt->second;
                if (inserted) {
                    blasInfo.mesh = mesh;
                    blasInfosChanged = true;
                }
                blasInfo.update = blasInfoUpdate;
                blasInfo.offset = int32_t(bindlessIdx->second);
                blasInfo.cullingDistanceSqr = mesh->rayTraceDistanceCulling * mesh->rayTraceDistanceCulling;
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

            // Only the skinned BLASes with a changed pose have joint matrices
            if (jointMatrices.empty())
                return;

            GPUProfiler::BeginThread("Skinning", commandList);
            GPUProfiler::BeginQuery("Skin BLASes");

            // The buffers might still be read by the previous frame
            commandList->MemoryBarrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            // Every work group of a pass looks up its BLAS in the group table, so each pass is a single dispatch
            const uint32_t skinGroupSize = 64;
            skinJobs.clear();
            skinGroupJobs.clear();

            for (auto& [entity, skinnedBlas] : skinnedBlases) {
                if (!skinnedBlas.skin)
                    continue;

                auto& blas = skinnedBlas.blas;
                auto job = GPUSkinJob {
                    .skinVertices = skinVertexBuffers[skinnedBlas.mesh.GetID()].Get()->GetDeviceAddress(),
                    .triangleVertices = skinnedBlas.triangleVertexBuffer.Get()->GetDeviceAddress(),
                    .target = blas->triangleBuffer.Get()->GetDeviceAddress(),
                    .bvhTriangles = blas->bvhTriangleBuffer.Get()->GetDeviceAddress(),
                    .firstGroup = uint32_t(skinGroupJobs.size()),
                    .elementCount = uint32_t(blas->triangleIndices.size()),
                    .jointOffset = skinnedBlas.jointOffset
                };

                auto groupCount = (job.elementCount + skinGroupSize - 1) / skinGroupSize;
                skinGroupJobs.insert(skinGroupJobs.end(), groupCount, uint32_t(skinJobs.size()));
                skinJobs.push_back(job);
            }

            auto triangleConstants = SkinPushConstants {
                .groupOffset = 0,
                .groupCount = uint32_t(skinGroupJobs.size())
            };

            if (skinJobBuffer.GetElementCount() < skinJobs.size())
                skinJobBuffer.SetSize(skinJobs.size());
            skinJobBuffer.SetData(skinJobs.data(), 0, skinJobs.size());
            if (skinGroupJobBuffer.GetElementCount() < skinGroupJobs.size())
                skinGroupJobBuffer.SetSize(skinGroupJobs.size());
            skinGroupJobBuffer.SetData(skinGroupJobs.data(), 0, skinGroupJobs.size());

            skinJobBuffer.Bind(commandList, 3, 0);
            jointMatricesBuffer.Bind(commandList, 3, 1);
            skinGroupJobBuffer.Bind(commandList, 3, 2);

            // The group count can exceed the limit of one dimension
            auto dispatchGroups = [&](uint32_t groupCount) {
                const uint32_t maxGroupsX = 32768;
                commandList->Dispatch(std::min(groupCount, maxGroupsX), (groupCount + maxGroupsX - 1) / maxGroupsX, 1);
            };

            auto pipeline = PipelineManager::GetPipeline(skinTrianglesPipelineConfig);
            commandList->BindPipeline(pipeline);
            commandList->PushConstants("constants", &triangleConstants);
            dispatchGroups(triangleConstants.groupCount);

            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            pipeline = PipelineManager::GetPipeline(refitBVHPipelineConfig);
            commandList->BindPipeline(pipeline);

            size_t levelCount = 0;
            for (const auto& [entity, skinnedBlas] : skinnedBlases)
                if (skinnedBlas.skin)
                    levelCount = std::max(levelCount, skinnedBlas.levelOffsets.size() - 1);

            // The levels of all BLASes are updated together, starting with the deepest one
            for (int32_t level = int32_t(levelCount) - 1; level >= 0; level--) {
                for (auto& [entity, skinnedBlas] : skinnedBlases) {
                    const auto& levelOffsets = skinnedBlas.levelOffsets;
                    if (!skinnedBlas.skin || level + 1 >= int32_t(levelOffsets.size()))
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

            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            GPUProfiler::EndQuery();
            GPUProfiler::EndThread();

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

                skinVertexBuffers[mesh.GetID()] = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                    Buffer::BufferUsageBits::DeviceAddressBit, sizeof(GPUSkinVertex), skinVertices.size(), skinVertices.data());
            }

            SkinnedBlas skinnedBlas = {
                .mesh = mesh,
                .blas = CreateRef<BLAS>()
            };

            std::vector<BLAS::Triangle> triangles;
            data.BuildBVHData(triangles);

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

            const auto& triangleIndices = skinnedBlas.blas->triangleIndices;
            std::vector<ivec4> triangleVertices(triangleIndices.size());
            for (size_t i = 0; i < triangleIndices.size(); i++) {
                auto idx = size_t(triangleIndices[i]) * 3;
                triangleVertices[i] = ivec4(data.indices[idx], data.indices[idx + 1], data.indices[idx + 2], 0);
            }

            skinnedBlas.triangleVertexBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                Buffer::BufferUsageBits::DeviceAddressBit, sizeof(ivec4), triangleVertices.size(), triangleVertices.data());

            return skinnedBlas;

        }

        void RayTracingWorld::UpdateDeformedInstances(const std::vector<ECS::Entity>& dirtyEntities) {

            auto renderState = &scene->renderState;
            const auto& gpuScene = renderState->gpuScene;

            auto& animationComponentPool = scene->entityManager.GetPool<AnimationComponent>();
            auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

            for (auto& [entity, instance] : deformedInstances) {
                instance.used = false;
                instance.active = false;
            }

            deformDispatches.clear();
            deformBlases.clear();
            jointMatrices.clear();

            auto hasCamera = scene->HasMainCamera();
            auto cameraLocation = hasCamera ? scene->GetMainCamera().GetLocation() : vec3(0.0f);

            // The configs are kept in the deform mesh, the graph parameters can move with every update
            std::unordered_set<size_t> updatedMeshDeformations;
            auto getDeformations = [&](const ResourceHandle<Mesh::Mesh>& mesh) -> const std::vector<SubDataDeformation>& {
                auto& deformations = deformMeshes[mesh.GetID()].deformations;
                if (!updatedMeshDeformations.insert(mesh.GetID()).second)
                    return deformations;

                const auto& subDatas = mesh->data.subData;
                deformations.resize(subDatas.size());
                for (size_t i = 0; i < subDatas.size(); i++) {
                    const auto& subData = subDatas[i];
                    auto& deformation = deformations[i];

                    const auto& graph = subData.material->graph;
                    auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
                    if (!compiled || !compiled->valid || compiled->deformFile.empty())
                        compiled = nullptr;

                    if (deformation.key == 0 || deformation.pending || deformation.graph != compiled) {
                        std::vector<std::string> macros;
                        if (mesh->data.IsSkinned())
                            macros.push_back("SKINNED");
                        if (mesh->vegetation)
                            macros.push_back("WIND");

                        // Graphs compile in the background, until then the sub mesh is deformed without them
                        deformation.config = PipelineConfig("raytracer/deform.csh", macros);
                        deformation.graph = compiled;
                        deformation.pending = false;
                        deformation.usesGraph = false;
                        if (compiled) {
                            auto graphConfig = PipelineConfig(compiled->deformFile, macros);
                            auto state = MaterialGraph::PipelineCache::Request(graphConfig, false);
                            if (state == MaterialGraph::PipelineCache::State::Ready) {
                                deformation.config = graphConfig;
                                deformation.usesGraph = true;
                            }
                            deformation.pending = state == MaterialGraph::PipelineCache::State::Pending;
                        }

                        deformation.key = deformation.config.shaderHash;
                        HashCombine(deformation.key, deformation.config.variantHash);
                    }

                    deformation.materialGraphOffset = 0;
                    if (deformation.usesGraph) {
                        auto graphData = renderState->materialGraphMap.find(subData.material.Get().get());
                        if (graphData != renderState->materialGraphMap.end())
                            deformation.materialGraphOffset = graphData->second.parameterOffset;
                    }
                }
                return deformations;
            };

            std::unordered_map<Hash, std::vector<GPUDeformJob>> variantJobs;
            std::unordered_map<Hash, PipelineConfig> variantConfigs;
            // Positions are offsets into the position buffer until it is allocated
            VkDeviceAddress positionCount = 0;

            auto addJobs = [&](DeformedInstance& instance, const mat4& globalMatrix, uint32_t jointOffset, bool created) {
                const auto& mesh = instance.mesh;
                const auto& deformMesh = deformMeshes[mesh.GetID()];
                const auto& deformations = getDeformations(mesh);

                auto positionOffset = positionCount * sizeof(vec3);
                positionCount += VkDeviceAddress(mesh->data.GetVertexCount());

                for (size_t i = 0; i < mesh->data.subData.size(); i++) {
                    const auto& subData = mesh->data.subData[i];
                    const auto& deformation = deformations[i];
                    auto [firstVertex, vertexCount] = deformMesh.subDataVertexRanges[i];
                    if (vertexCount == 0)
                        continue;

                    variantJobs[deformation.key].push_back(GPUDeformJob {
                        .matrix = mat3x4(glm::transpose(globalMatrix)),
                        .sourceVertices = deformMesh.vertexBuffer.Get()->GetDeviceAddress(),
                        .positions = positionOffset,
                        .normals = instance.normalBuffer.Get()->GetDeviceAddress(),
                        .vertexOffset = firstVertex,
                        .vertexCount = vertexCount,
                        .jointOffset = jointOffset,
                        .windBendScale = mesh->windBendScale,
                        .windWiggleScale = mesh->windWiggleScale,
                        .windTextureLod = mesh->windNoiseTextureLod,
                        .materialGraphOffset = deformation.materialGraphOffset,
                        .invertUVs = mesh->invertUVs ? 1u : 0u,
                        .uvTiling = subData.material->tiling,
                        .uvAnimation = subData.material->uvAnimation
                    });
                    variantConfigs.try_emplace(deformation.key, deformation.config);
                }

                // A BLAS which was never built can't be refit, recording checks that again
                deformBlases.push_back(DeformBlas {
                    .blas = instance.blas,
                    .vertexAddress = positionOffset,
                    .refit = !created && instance.blas->allowUpdate
                });
            };

            CPUProfiler::Scope profilerScope("Skinned instances");
            for (size_t i = 0; i < animationComponentPool.GetCount(); i++) {
                auto entity = animationComponentPool[i];
                const auto& animationComponent = animationComponentPool.GetByIndex(i);
                auto meshComponent = meshComponentPool.TryGet(entity);
                auto transformComponent = transformComponentPool.TryGet(entity);

                if (!meshComponent || !transformComponent || !meshComponent->visible || !meshComponent->mesh.IsLoaded() ||
                    !meshComponent->mesh->rayTrace || !meshComponent->mesh->data.IsSkinned() || animationComponent.jointMatrices.empty())
                    continue;

                auto item = deformedInstances.find(entity);
                // Entities which already have a BLAS keep it a little further out, so it isn't recreated at the border
                auto distance = item != deformedInstances.end() ? skinnedBlasDistance * 1.25f : skinnedBlasDistance;
                if (hasCamera && glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation) > distance * distance)
                    continue;

                const auto& mesh = meshComponent->mesh;
                // The BLAS needs to be created again when switching between refits and builds
                auto created = item == deformedInstances.end() || item->second.mesh.GetID() != mesh.GetID() ||
                    item->second.meshBlas != mesh->blas || item->second.blas->allowUpdate != refitSkinnedBlases;
                if (created)
                    item = deformedInstances.insert_or_assign(entity, CreateDeformedInstance(mesh)).first;

                auto& instance = item->second;
                instance.used = true;
                instance.active = true;
                instance.deformedStatic = false;
                // Poses that didn't change keep their BLAS and normals
                if (!created && instance.blas->isBuilt && !animationComponent.IsPoseUpdated())
                    continue;

                auto jointOffset = uint32_t(jointMatrices.size());
                jointMatrices.insert(jointMatrices.end(), animationComponent.jointMatrices.begin(),
                    animationComponent.jointMatrices.end());
                addJobs(instance, transformComponent->globalMatrix, jointOffset, created);
            }

            profilerScope.Next("Deformed instances");
            auto activeDistanceSqr = deformedBlasDistance * deformedBlasDistance;
            auto keepDistance = deformedBlasDistance * 1.25f;

            // The deformed meshes and the octree query are expensive in large scenes, they are only searched again
            // once the camera moved a bit, the meshes of the GPU scene changed or after a while (compiled graphs join)
            auto candidateMargin = 0.1f * deformedBlasDistance;
            if (deformCandidateFrames++ >= 60 || deformCandidateSlotCount != gpuScene.slots.size() ||
                glm::distance2(cameraLocation, deformCandidateLocation) > candidateMargin * candidateMargin) {
                // Static meshes are only deformed near the camera, the GPU scene writes all instances without one
                deformedMeshes.clear();
                for (const auto& slot : gpuScene.slots) {
                    if (hasCamera && !slot.skinned && slot.mesh.IsLoaded() && slot.mesh->rayTrace &&
                        (slot.mesh->vegetation || IsDeformedByMaterial(slot.mesh)))
                        deformedMeshes.insert(slot.mesh.GetID());
                }

                deformCandidates.clear();
                if (!deformedMeshes.empty()) {
                    auto queryDistance = vec3(keepDistance + candidateMargin);
                    for (ECS::Entity entity : scene->SpacePartitioning::QueryAABB(
                        Volume::AABB(cameraLocation - queryDistance, cameraLocation + queryDistance))) {
                        auto meshComponent = meshComponentPool.TryGet(entity);
                        if (meshComponent && meshComponent->mesh.IsLoaded() && deformedMeshes.contains(meshComponent->mesh.GetID()))
                            deformCandidates.push_back(entity);
                    }
                }
                std::sort(deformCandidates.begin(), deformCandidates.end());
                deformCandidateLocation = cameraLocation;
                deformCandidateSlotCount = gpuScene.slots.size();
                deformCandidateFrames = 0;
            }
            else {
                // Entities created, moved or changed since the last search join right away, the GPU scene leaves them
                // out as soon as they're near the camera
                auto queryDistance = vec3(keepDistance + candidateMargin);
                for (auto entity : dirtyEntities) {
                    auto meshComponent = meshComponentPool.TryGet(entity);
                    auto transformComponent = transformComponentPool.TryGet(entity);
                    if (!meshComponent || !transformComponent || !meshComponent->mesh.IsLoaded() ||
                        !deformedMeshes.contains(meshComponent->mesh.GetID()))
                        continue;

                    auto offset = glm::abs(vec3(transformComponent->globalMatrix[3]) - deformCandidateLocation);
                    if (glm::any(glm::greaterThan(offset, queryDistance)))
                        continue;

                    auto candidate = std::lower_bound(deformCandidates.begin(), deformCandidates.end(), entity);
                    if (candidate == deformCandidates.end() || *candidate != entity)
                        deformCandidates.insert(candidate, entity);
                }
            }

            for (auto entity : deformCandidates) {
                auto meshComponent = meshComponentPool.TryGet(entity);
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!meshComponent || !transformComponent || !meshComponent->visible || !meshComponent->mesh.IsLoaded() ||
                    !deformedMeshes.contains(meshComponent->mesh.GetID()))
                    continue;

                // Has to be the same distance as the one of the GPU scene, which leaves these instances out
                auto distanceSqr = glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation);
                auto item = deformedInstances.find(entity);
                // Skinned instances were deformed above
                if (item != deformedInstances.end() && item->second.used)
                    continue;
                if (distanceSqr > (item != deformedInstances.end() ? keepDistance * keepDistance : activeDistanceSqr))
                    continue;

                const auto& mesh = meshComponent->mesh;
                auto created = item == deformedInstances.end() || item->second.mesh.GetID() != mesh.GetID() ||
                    item->second.meshBlas != mesh->blas;
                if (created)
                    item = deformedInstances.insert_or_assign(entity, CreateDeformedInstance(mesh)).first;

                auto& instance = item->second;
                instance.used = true;
                instance.deformedStatic = true;
                instance.active = distanceSqr < activeDistanceSqr;
                if (instance.active)
                    addJobs(instance, transformComponent->globalMatrix, 0, created);
            }

            profilerScope.Next("Deformation jobs");
            std::erase_if(deformedInstances, [](const auto& item) { return !item.second.used; });
            std::unordered_set<size_t> usedDeformMeshes;
            for (const auto& [entity, instance] : deformedInstances)
                usedDeformMeshes.insert(instance.mesh.GetID());
            std::erase_if(deformMeshes, [&](const auto& item) { return !usedDeformMeshes.contains(item.first); });

            if (positionCount == 0)
                return;

            // The positions are only read while the BLASes are built in this frame
            auto positionSize = size_t(positionCount) * sizeof(vec3);
            if (!deformPositionBuffer || deformPositionBuffer->size < positionSize) {
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .domain = Graphics::BufferDomain::Device,
                    .size = positionSize + positionSize / 4
                };
                deformPositionBuffer = Graphics::GraphicsDevice::DefaultDevice->CreateBuffer(desc);
            }

            auto positionAddress = deformPositionBuffer->GetDeviceAddress();
            for (auto& deformBlas : deformBlases)
                deformBlas.vertexAddress += positionAddress;

            // Every work group looks up its job in the group table, so each pipeline is a single dispatch
            const uint32_t deformGroupSize = 64;
            std::vector<GPUDeformJob> jobs;
            std::vector<uint32_t> groupJobs;
            for (auto& [key, variant] : variantJobs) {
                auto groupOffset = uint32_t(groupJobs.size());
                for (auto& job : variant) {
                    job.positions += positionAddress;
                    job.firstGroup = uint32_t(groupJobs.size()) - groupOffset;
                    auto groupCount = (job.vertexCount + deformGroupSize - 1) / deformGroupSize;
                    groupJobs.insert(groupJobs.end(), groupCount, uint32_t(jobs.size()));
                    jobs.push_back(job);
                }
                deformDispatches.push_back(DeformDispatch {
                    .config = variantConfigs[key],
                    .groupOffset = groupOffset,
                    .groupCount = uint32_t(groupJobs.size()) - groupOffset
                });
            }

            deformJobUpload = uploadBufferPool.Upload(jobs.data(), jobs.size() * sizeof(GPUDeformJob));
            deformGroupJobUpload = uploadBufferPool.Upload(groupJobs.data(), groupJobs.size() * sizeof(uint32_t));
            jointMatrixUpload = jointMatrices.empty() ? nullptr :
                uploadBufferPool.Upload(jointMatrices.data(), jointMatrices.size() * sizeof(mat3x4));

        }

        void RayTracingWorld::RecordDeformation(Graphics::CommandList* commandList) {

            auto& pending = *pendingInstances;
            if (pending.deformDispatches.empty())
                return;

            GPUProfiler::BeginQuery("Deform instances");

            // The buffers might still be read by the previous frame
            commandList->MemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            commandList->BindBuffer(pending.deformJobBuffer, 3, 0);
            // Without skinned instances any buffer works
            commandList->BindBuffer(pending.jointMatrixBuffer ? pending.jointMatrixBuffer : pending.deformJobBuffer, 3, 1);
            commandList->BindBuffer(pending.deformGroupJobBuffer, 3, 2);
            scene->renderState.wind.noiseMap.Bind(commandList, 3, 3);
            scene->renderState.materialGraphBuffer.Bind(commandList, 3, 9);

            for (auto& dispatch : pending.deformDispatches) {
                auto pipeline = PipelineManager::GetPipeline(dispatch.config);
                commandList->BindPipeline(pipeline);

                auto constants = SkinPushConstants {
                    .groupOffset = dispatch.groupOffset,
                    .groupCount = dispatch.groupCount
                };
                commandList->PushConstants("constants", &constants);

                // The group count can exceed the limit of one dimension
                const uint32_t maxGroupsX = 32768;
                commandList->Dispatch(std::min(dispatch.groupCount, maxGroupsX), (dispatch.groupCount + maxGroupsX - 1) / maxGroupsX, 1);
            }

            // The positions are read by the BLAS builds, the normals by the hits
            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            std::vector<Ref<Graphics::BLAS>> buildBlases, refitBlases;
            for (const auto& deformBlas : pending.deformBlases) {
                deformBlas.blas->SetVertexData(deformBlas.vertexAddress, sizeof(vec3));
                if (deformBlas.refit && deformBlas.blas->isBuilt)
                    refitBlases.push_back(deformBlas.blas);
                else
                    buildBlases.push_back(deformBlas.blas);
            }

            GPUProfiler::EndAndBeginQuery("Build deformed BLASes");
            if (!buildBlases.empty())
                deformBlasBuilder.BuildBLAS(buildBlases, commandList);
            GPUProfiler::EndAndBeginQuery("Refit deformed BLASes");
            if (!refitBlases.empty())
                deformBlasBuilder.UpdateBLAS(refitBlases, commandList);
            GPUProfiler::EndQuery();

            commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        }

        RayTracingWorld::DeformedInstance RayTracingWorld::CreateDeformedInstance(const ResourceHandle<Mesh::Mesh>& mesh) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            auto& data = mesh->data;
            auto vertexCount = size_t(data.GetVertexCount());

            auto deformMeshItem = deformMeshes.find(mesh.GetID());
            if (deformMeshItem == deformMeshes.end() || deformMeshItem->second.meshBlas != mesh->blas) {
                auto hasTangents = data.tangents.ContainsData();
                auto hasTexCoords = data.texCoords.ContainsData();
                auto hasColors = data.colors.ContainsData();
                auto isSkinned = data.IsSkinned();

                std::vector<GPUDeformVertex> vertices(vertexCount);
                for (size_t i = 0; i < vertexCount; i++) {
                    auto& vertex = vertices[i];
                    auto packedNormal = Common::Packing::PackSignedVector3x10_1x2(vec4(vec3(data.normals[i]), 0.0f));
                    vertex.position = vec4(data.vertices[i], reinterpret_cast<float&>(packedNormal));
                    if (hasTangents)
                        vertex.tangent = uint32_t(Common::Packing::PackSignedVector3x10_1x2(data.tangents[i]));
                    if (hasTexCoords)
                        vertex.texCoord = glm::packHalf2x16(data.texCoords[i]);
                    // The rasterizer uses white without vertex colors
                    vertex.color = hasColors ? glm::packUnorm4x8(data.colors[i]) : 0xFFFFFFFFu;
                    if (isSkinned) {
                        auto joints = glm::uvec4(data.joints[i]);
                        vertex.joints01 = joints.x | (joints.y << 16);
                        vertex.joints23 = joints.z | (joints.w << 16);
                        vertex.weights = glm::packUnorm4x8(data.weights[i]);
                    }
                }

                auto& deformMesh = deformMeshes[mesh.GetID()];
                deformMesh.meshBlas = mesh->blas;
                deformMesh.deformations.clear();
                deformMesh.subDataVertexRanges.clear();
                deformMesh.vertexBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                    Buffer::BufferUsageBits::DeviceAddressBit, sizeof(GPUDeformVertex), vertices.size(), vertices.data());

                // Each sub mesh is deformed with the pipeline of its material
                for (const auto& subData : data.subData) {
                    uint32_t firstVertex = std::numeric_limits<uint32_t>::max(), lastVertex = 0;
                    for (uint32_t i = 0; i < subData.indicesCount; i++) {
                        auto index = data.indices[subData.indicesOffset + i];
                        firstVertex = std::min(firstVertex, index);
                        lastVertex = std::max(lastVertex, index);
                    }
                    deformMesh.subDataVertexRanges.emplace_back(subData.indicesCount ? firstVertex : 0u,
                        subData.indicesCount ? lastVertex - firstVertex + 1 : 0u);
                }
            }

            std::vector<Graphics::ASGeometryRegion> geometryRegions;
            for (auto& subData : data.subData) {
                geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                    .indexCount = subData.indicesCount,
                    .indexOffset = subData.indicesOffset,
                    .opaque = !subData.material->opacityMap.IsValid() && subData.material->opacity == 1.0f &&
                        !subData.material->HasGraph()
                    });
            }

            // The vertices are written into the position buffer of a frame before every build, see SetVertexData
            Graphics::ASBuilder asBuilder;
            auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(deformMeshes[mesh.GetID()].vertexBuffer.Get(),
                mesh->indexBuffer.buffer, vertexCount, sizeof(vec3), mesh->indexBuffer.elementSize, geometryRegions);
            blasDesc.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
            blasDesc.isDynamic = true;
            blasDesc.allowUpdate = refitSkinnedBlases;

            return DeformedInstance {
                .mesh = mesh,
                .meshBlas = mesh->blas,
                .blas = device->CreateBLAS(blasDesc),
                .normalBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DeviceAddressBit,
                    2 * sizeof(uint32_t), vertexCount)
            };

        }

        bool RayTracingWorld::IsDeformedByMaterial(const ResourceHandle<Mesh::Mesh>& mesh) const {

            return std::any_of(mesh->data.subData.begin(), mesh->data.subData.end(), [](const auto& subData) {
                const auto& graph = subData.material->graph;
                auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
                return compiled && compiled->valid && !compiled->deformFile.empty();
                });

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
