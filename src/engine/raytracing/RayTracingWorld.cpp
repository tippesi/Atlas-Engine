#include "RayTracingWorld.h"
#include "materialgraph/NodeLibrary.h"
#include "common/MathHelper.h"
#include "scene/Scene.h"

#include "mesh/MeshData.h"
#include "volume/BVH.h"
#include "graphics/ASBuilder.h"
#include "profiler/CPUProfiler.h"
#include "profiler/GPUProfiler.h"
#include "pipeline/PipelineManager.h"
#include "common/ColorConverter.h"

#include <glm/gtc/packing.hpp>
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

            materialBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUPackedMaterial));
            bvhInstanceBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHInstance));
            tlasNodeBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHNode));
            lastMatricesBuffer = Buffer::Buffer(bufferUsage, sizeof(mat4x3));

            deformTrianglesPipelineConfig = PipelineConfig("raytracer/deformTriangles.csh");
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
            const std::vector<ECS::Entity>& dirtyEntities) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            if (!device->swapChain->isComplete) return;
            if (!subset.Any() && !scene->terrain.IsLoaded()) return;

            auto renderState = &scene->renderState;

            CPUProfiler::Scope profilerScope("Blas infos");
            buildBlases.clear();
            std::vector<Ref<BLAS>> refreshedBlases;

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
                    blasInfosChanged = true;
                }

                blasInfo.update = blasInfoUpdate;
                blasInfo.offset = int32_t(bindlessIdx->second);
                blasInfo.cullingDistanceSqr = mesh->rayTraceDistanceCulling * mesh->rayTraceDistanceCulling;

                // Some extra path for hardware raytracing, don't want to do work twice
                if (hardwareRayTracing) {
                    if (mesh->blas->needsBvhRefresh && mesh->blas->blas->isDynamic) {
                        buildBlases.push_back(mesh->blas->blas);
                        refreshedBlases.push_back(mesh->blas);
                    }

                    blasInfo.idx = meshCount++;
                }

                // The levels of detail have the materials of the mesh
                for (const auto& lodBlas : mesh->lodBlases) {
                    auto lodBindlessIdx = renderState->blasToBindlessIdx.find(lodBlas);
                    if (lodBindlessIdx == renderState->blasToBindlessIdx.end())
                        continue;

                    auto [lodInfoIt, lodInserted] = blasInfos.try_emplace(lodBlas);
                    auto& lodInfo = lodInfoIt->second;
                    lodInfo.mesh = mesh;
                    blasInfosChanged |= lodInserted;

                    lodInfo.update = blasInfoUpdate;
                    lodInfo.offset = int32_t(lodBindlessIdx->second);
                    lodInfo.cullingDistanceSqr = blasInfo.cullingDistanceSqr;
                    if (hardwareRayTracing)
                        lodInfo.idx = meshCount++;
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
            UpdateDeformedInstances(dirtyEntities);

            std::erase_if(blasInfos, [&](auto& entry) {
                if (entry.second.update == blasInfoUpdate)
                    return false;
                ReleaseHitRecords(entry.second);
                blasInfosChanged = true;
                return true;
                });

            // The deformation is recorded while rendering, see RecordDeformation
            if (hardwareRayTracing && !buildBlases.empty()) {
                auto commandList = device->GetCommandList();
                commandList->BeginCommands();
                GPUProfiler::BeginThread("Dynamic BLASes", commandList);
                GPUProfiler::BeginQuery("Build BLASes");
                // One batch is built per frame, the rest stays requested for the next frames
                auto builtCount = blasBuilder.BuildBLAS(buildBlases, commandList);
                for (int32_t i = 0; i < builtCount; i++)
                    refreshedBlases[i]->needsBvhRefresh = false;
                GPUProfiler::EndQuery();
                GPUProfiler::EndThread();
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

        }

        uint32_t RayTracingWorld::GetInstanceMask(const Mesh::Mesh& mesh) {

            auto translucent = !mesh.data.subData.empty() && std::all_of(mesh.data.subData.begin(), mesh.data.subData.end(),
                [](const auto& subData) { return subData.material.IsLoaded() && subData.material->IsTranslucent(); });
            if (translucent)
                return InstanceCullMasks::MaskTranslucent | (mesh.castShadow ? InstanceCullMasks::MaskTranslucentShadow : 0u);
            return InstanceCullMasks::MaskAll | (mesh.castShadow ? InstanceCullMasks::MaskShadow : 0u);

        }

        GPUPackedMaterial RayTracingWorld::PackMaterial(const GPUMaterial& material) {

            auto packTextures = [](int32_t first, int32_t second) {
                return uint32_t(first < 0 ? 0xFFFF : first) | (uint32_t(second < 0 ? 0xFFFF : second) << 16u);
                };
            auto flags = (material.invertUVs ? 1u : 0u) | (material.twoSided ? 2u : 0u) | (material.cullBackFaces ? 4u : 0u) |
                (material.useVertexColors ? 8u : 0u);

            return GPUPackedMaterial {
                .baseColor = uint32_t(Common::Packing::PackUnsignedVector3x10_1x2(vec4(::Helper::Math::Saturate(material.baseColor), 0.0f))),
                .emissiveRedGreen = glm::packHalf2x16(vec2(material.emissiveColor.r, material.emissiveColor.g)),
                .emissiveBlueThickness = glm::packHalf2x16(vec2(material.emissiveColor.b, material.thickness)),
                .opacityRoughnessMetalnessAo = glm::packUnorm4x8(vec4(material.opacity, material.roughness, material.metalness, material.ao)),
                .reflectanceNormalScaleFlags = (glm::packUnorm4x8(vec4(material.reflectance, material.normalScale, 0.0f, 0.0f)) & 0xFFFFu) |
                    (flags << 16u) | (material.blendMode << 24u),
                .tiling = glm::packHalf2x16(vec2(material.tiling, material.terrainTiling)),
                .uvAnimation = glm::packHalf2x16(vec2(material.uvAnimationX, material.uvAnimationY)),
                .textures = { packTextures(material.baseColorTexture, material.opacityTexture),
                    packTextures(material.normalTexture, material.roughnessTexture),
                    packTextures(material.metalnessTexture, material.aoTexture),
                    packTextures(material.emissiveTexture, material.terrainNormalTexture) },
                .transmissionColor = material.transmissionColor,
                .idGraphHitGroup = (uint32_t(material.ID) & 0xFFFFu) | (material.graphHitGroup << 16u),
                .graphParameterOffset = material.graphParameterOffset,
                .refractiveIndexAttenuationDistance = glm::packHalf2x16(vec2(material.refractiveIndex, material.attenuationDistance)),
                .attenuationColor = material.attenuationColor
            };

        }

        void RayTracingWorld::UpdateMaterials() {

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
            // without an opacity use the any hit stage of the other materials. Only ray tracing pipelines have hit groups
            auto isCompiled = [](const std::string& file, VkShaderStageFlagBits stage) {
                return file.empty() || MaterialGraph::PipelineCache::RequestStage(file, stage) == MaterialGraph::PipelineCache::State::Ready;
            };
            std::set<MaterialGraphHitGroup> hitGroups;
            for (const auto& [material, data] : sceneState->materialGraphMap) {
                auto compiled = static_cast<Material*>(material)->graph->GetCompiled();
                if (hardwareRayTracing && compiled && !compiled->hitFile.empty() &&
                    isCompiled(compiled->hitFile, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) &&
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
                            opaque &= !material->IsTranslucent() && (material->HasGraph() ? opaqueGraphs.contains(material.Get().get()) :
                                !material->opacityMap.IsValid() && material->opacity == 1.0f);
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

                        // Graphs hold these values in their surface output, hit groups replace them per hit
                        using MaterialGraph::NodeLibrary;
                        const auto& graph = material->graph;
                        auto hasGraph = material->HasGraph();
                        auto transmission = hasGraph ? vec3(graph->GetSurfaceValue(NodeLibrary::baseColorPin)) *
                            graph->GetSurfaceValue(NodeLibrary::transmissionPin).x :
                            Common::ColorConverter::ConvertSRGBToLinear(material->transmissiveColor);
                        auto attenuationColor = hasGraph ? vec3(graph->GetSurfaceValue(NodeLibrary::attenuationColorPin)) :
                            Common::ColorConverter::ConvertSRGBToLinear(material->attenuationColor);
                        auto refractiveIndex = hasGraph ? graph->GetSurfaceValue(NodeLibrary::refractiveIndexPin).x : material->refractiveIndex;
                        auto attenuationDistance = hasGraph ? graph->GetSurfaceValue(NodeLibrary::attenuationDistancePin).x :
                            material->attenuationDistance;
                        auto thickness = hasGraph ? graph->GetSurfaceValue(NodeLibrary::thicknessPin).x : material->thickness;

                        gpuMaterial.baseColor = Common::ColorConverter::ConvertSRGBToLinear(material->baseColor);
                        gpuMaterial.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(
                            vec4(Helper::Math::Saturate(transmission), 0.0f));
                        gpuMaterial.emissiveColor = Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor)
                            * material->emissiveIntensity;

                        gpuMaterial.opacity = material->opacity;
                        gpuMaterial.blendMode = uint32_t(material->GetBlendMode());
                        // Refracting graphs need an index other than 1, the one of their hit decides
                        gpuMaterial.refractiveIndex = material->Refracts() ? (refractiveIndex != 1.0f ? refractiveIndex : 1.5f) : 1.0f;
                        gpuMaterial.attenuationColor = Common::Packing::PackUnsignedVector3x10_1x2(
                            vec4(Helper::Math::Saturate(attenuationColor), 0.0f));
                        gpuMaterial.attenuationDistance = material->Refracts() ? attenuationDistance : 0.0f;
                        // Rays enter two sided surfaces from both sides, they can't be volumes. A graph with a linked
                        // thickness gives it per hit
                        gpuMaterial.thickness = material->Refracts() && material->IsTwoSided() ? std::max(thickness, 0.001f) : 0.0f;

                        gpuMaterial.roughness = material->roughness;
                        gpuMaterial.metalness = material->metalness;
                        gpuMaterial.ao = material->ao;

                        gpuMaterial.reflectance = hasGraph ? graph->GetSurfaceValue(NodeLibrary::reflectancePin).x : material->reflectance;

                        gpuMaterial.normalScale = material->normalScale;

                        gpuMaterial.tiling = material->tiling;
                        gpuMaterial.uvAnimationX = material->uvAnimation.x;
                        gpuMaterial.uvAnimationY = material->uvAnimation.y;
                        gpuMaterial.terrainTiling = blasInfo.node ? 1.0f / blasInfo.node->sideLength : 1.0f;

                        gpuMaterial.invertUVs = mesh.IsLoaded() ? (mesh->invertUVs ? 1 : 0) : 0;
                        gpuMaterial.twoSided = material->IsTwoSided() ? 1 : 0;
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

                    materials[blasInfo.materialOffset + meshMaterialID - 1] = PackMaterial(gpuMaterial);
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
            deformedInstances.clear();
            deformMeshes.clear();
            pendingInstances.reset();
            pendingDeformation = PendingDeformation();

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

            // Without instances there is no TLAS to bind, the pending one is built before the ray traced passes
            auto hasTLAS = !hardwareRayTracing || tlas || GetPendingInstances();
            return materialBuffer.GetSize() > 0 && !blasInfos.empty() && hasTLAS;

        }

        void RayTracingWorld::UpdateForSoftwareRayTracing(vec3 cameraLocation, bool hasCamera) {

            auto renderState = &scene->renderState;
            auto focalLength = renderState->gpuScene.GetFocalLength();

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
                    auto deformed = deformedInstances.find(entity);
                    if (deformed != deformedInstances.end() && deformed->second.active &&
                        renderState->blasToBindlessIdx.contains(deformed->second.softwareBlas))
                        blas = deformed->second.softwareBlas;

                    if (!renderState->blasToBindlessIdx.contains(blas) || blas->needsBvhRefresh)
                        return;

                    auto transformComponent = transformComponentPool.TryGet(entity);
                    if (!transformComponent)
                        return;

                    // The level of detail which is rasterized, as far as its BLAS is ready
                    const auto& lodBlases = meshComponent.mesh->lodBlases;
                    if (blas == meshComponent.mesh->blas && !lodBlases.empty()) {
                        auto lod = std::min(size_t(meshComponent.mesh->SelectLod(transformComponent->globalMatrix,
                            meshComponent.aabb, cameraLocation, focalLength, scene->meshLodErrorScale, scene->forcedMeshLod)), lodBlases.size());
                        for (; lod > 0; lod--) {
                            const auto& lodBlas = lodBlases[lod - 1];
                            if (renderState->blasToBindlessIdx.contains(lodBlas) && !lodBlas->needsBvhRefresh) {
                                blas = lodBlas;
                                break;
                            }
                        }
                    }

                    auto& blasInfo = blasInfos[blas];
                    auto distSqd = glm::distance2(
                        vec3(transformComponent->globalMatrix[3]),
                        cameraLocation);
                    if (hasCamera && distSqd > blasInfo.cullingDistanceSqr)
                        return;

                    jobContext.instanceAABBs.push_back(meshComponent.aabb);
                    auto inverseMatrix = mat3x4(glm::transpose(transformComponent->inverseGlobalMatrix));

                    GPUBVHInstance gpuBvhInstance = {
                        .inverseMatrix = inverseMatrix,
                        .meshOffset = blasInfo.offset,
                        .materialOffset = blasInfo.materialOffset,
                        .mask = GetInstanceMask(*meshComponent.mesh.Get())
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

            // The instances of the GPU scene reference their mesh by slot, the levels of detail come after the slots
            size_t gpuInstanceCount = 0;
            rtMeshes.resize(gpuScene.slots.size());
            for (size_t i = 0; i < gpuScene.slots.size(); i++) {
                const auto& slot = gpuScene.slots[i];

                auto getRTMesh = [&](const Ref<BLAS>& blas) {
                    const auto& blasInfo = blasInfos[blas];
                    return GPURTMesh {
                        .blasOffset = uint32_t(blasInfo.offset),
                        .materialOffset = blasInfo.materialOffset,
                        .mask = GetInstanceMask(*slot.mesh.Get()),
                        .flags = rtMeshUsable | (blasInfo.forceOpaque ? rtMeshForceOpaque : 0u),
                        .cullingDistanceSqr = blasInfo.cullingDistanceSqr,
                        .deformDistanceSqr = deformedMeshes.contains(slot.mesh.GetID()) && hasCamera ?
                            deformedBlasDistance * deformedBlasDistance : 0.0f,
                        .blasAddress = blas->blas->deviceAddress,
                        .hitRecordOffset = blasInfo.hitRecordOffset
                    };
                };

                GPURTMesh rtMesh;
                if (slot.geometry && slot.mesh->rayTrace && isUsable(slot.mesh->blas)) {
                    rtMesh = getRTMesh(slot.mesh->blas);
                    rtMesh.lodOffset = uint32_t(rtMeshes.size());
                    // The culling chooses from the levels of the slot, the ones from the first unusable BLAS on are left out
                    const auto& lodBlases = slot.mesh->lodBlases;
                    for (size_t lod = 1; lod < slot.lodCount && lod <= lodBlases.size() && isUsable(lodBlases[lod - 1]); lod++) {
                        rtMeshes.push_back(getRTMesh(lodBlases[lod - 1]));
                        rtMesh.lodCount++;
                    }
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
                    const auto& indexBuffer = deformed->indexBuffer;
                    gpuBvhInstance.deformedVertices = deformed->normalBuffer.Get()->GetDeviceAddress();
                    gpuBvhInstance.meshIndices = (indexBuffer.buffer->GetDeviceAddress() + indexBuffer.offset) |
                        (indexBuffer.elementSize == 2 ? 1 : 0);
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
                addInstance(node->cell->blas, globalMatrix, globalMatrix, uint32_t(InstanceCullMasks::MaskAll | InstanceCullMasks::MaskShadow));
            }

            for (const auto& slot : gpuScene.slots) {
                if (!slot.instanceVertices || !slot.mesh->rayTrace)
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
                        GetInstanceMask(*slot.mesh.Get()), deformedInstance);
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
                    GetInstanceMask(*instance.mesh.Get()), instance.blas->isBuilt ? &instance : nullptr);
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

        void RayTracingWorld::UpdateDeformedInstances(const std::vector<ECS::Entity>& dirtyEntities) {

            auto renderState = &scene->renderState;

            auto& animationComponentPool = scene->entityManager.GetPool<AnimationComponent>();
            auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();

            for (auto& [entity, instance] : deformedInstances) {
                instance.used = false;
                instance.active = false;
            }

            PendingDeformation frameDeformation;
            jointMatrices.clear();
            softBodyVertices.clear();

            auto isBuilt = [&](const DeformedInstance& instance) {
                return hardwareRayTracing ? bool(instance.blas->isBuilt) : instance.softwareBlas->IsBuilt();
            };

            auto hasCamera = scene->HasMainCamera();
            auto cameraLocation = hasCamera ? scene->GetMainCamera().GetLocation() : vec3(0.0f);

            // The configs are kept in the deform mesh, the graph parameters can move with every update
            std::set<std::pair<size_t, bool>> updatedMeshDeformations;
            auto getDeformations = [&](const ResourceHandle<Mesh::Mesh>& mesh, bool softBody) -> const std::vector<SubDataDeformation>& {
                auto& deformMesh = deformMeshes[mesh.GetID()];
                auto& deformations = softBody ? deformMesh.softBodyDeformations : deformMesh.deformations;
                if (!updatedMeshDeformations.insert({ mesh.GetID(), softBody }).second)
                    return deformations;

                const auto& subDatas = mesh->data.subData;
                deformations.resize(subDatas.size());
                for (size_t i = 0; i < subDatas.size(); i++) {
                    const auto& subData = subDatas[i];
                    auto& deformation = deformations[i];

                    // Materials which failed to load have no graph
                    auto graph = subData.material.IsLoaded() ? subData.material->graph : ResourceHandle<MaterialGraph::Graph>();
                    auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
                    if (!compiled || !compiled->valid || compiled->deformFile.empty())
                        compiled = nullptr;

                    if (deformation.key == 0 || deformation.pending || deformation.graph != compiled) {
                        std::vector<std::string> macros;
                        if (mesh->data.IsSkinned())
                            macros.push_back("SKINNED");
                        else if (softBody)
                            macros.push_back("SOFT_BODY");
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
            std::vector<GPUDeformTriangleJob> triangleJobs;
            // Positions and software normals are offsets into the vertex buffer until it is allocated
            VkDeviceAddress positionCount = 0;

            // The joint matrices of skinned meshes or the vertices of soft bodies start at the source offset
            auto addJobs = [&](DeformedInstance& instance, const mat4& globalMatrix, uint32_t sourceOffset, bool created,
                bool softBody) {
                const auto& mesh = instance.mesh;
                const auto& deformMesh = deformMeshes[mesh.GetID()];
                const auto& deformations = getDeformations(mesh, softBody);

                auto positionOffset = positionCount * sizeof(vec3);
                // Software ray tracing reads the normals only while gathering the triangles
                auto normals = hardwareRayTracing ? instance.normalBuffer.Get()->GetDeviceAddress() :
                    positionCount * sizeof(glm::uvec2);
                positionCount += VkDeviceAddress(mesh->data.GetVertexCount() - mesh->data.lodVertexCount);

                for (size_t i = 0; i < mesh->data.subData.size(); i++) {
                    const auto& subData = mesh->data.subData[i];
                    const auto& deformation = deformations[i];
                    auto materialLoaded = subData.material.IsLoaded();
                    auto [firstVertex, vertexCount] = deformMesh.subDataVertexRanges[i];
                    if (vertexCount == 0)
                        continue;

                    variantJobs[deformation.key].push_back(GPUDeformJob {
                        .matrix = mat3x4(glm::transpose(globalMatrix)),
                        .sourceVertices = deformMesh.vertexBuffer.Get()->GetDeviceAddress(),
                        .positions = positionOffset,
                        .normals = normals,
                        .vertexOffset = firstVertex,
                        .vertexCount = vertexCount,
                        .deformOffset = sourceOffset,
                        .windBendScale = mesh->windBendScale,
                        .windWiggleScale = mesh->windWiggleScale,
                        .windTextureLod = mesh->windNoiseTextureLod,
                        .materialGraphOffset = deformation.materialGraphOffset,
                        .invertUVs = mesh->invertUVs ? 1u : 0u,
                        .uvTiling = materialLoaded ? subData.material->tiling : 1.0f,
                        .uvAnimation = materialLoaded ? subData.material->uvAnimation : vec2(0.0f),
                        .hasTexCoords1 = mesh->data.texCoords1.ContainsData() ? 1u : 0u
                    });
                    variantConfigs.try_emplace(deformation.key, deformation.config);
                }

                if (hardwareRayTracing) {
                    // A BLAS which was never built can't be refit, recording checks that again
                    frameDeformation.blases.push_back(DeformBlas {
                        .blas = instance.blas,
                        .vertexAddress = positionOffset,
                        .refit = !created && instance.blas->allowUpdate
                    });
                    return;
                }

                const auto& softwareBlas = instance.softwareBlas;
                triangleJobs.push_back(GPUDeformTriangleJob {
                    .triangleVertices = deformMesh.triangleVertexBuffer.Get()->GetDeviceAddress(),
                    .positions = positionOffset,
                    .normals = normals,
                    .triangles = softwareBlas->triangleBuffer.Get()->GetDeviceAddress(),
                    .bvhTriangles = softwareBlas->bvhTriangleBuffer.Get()->GetDeviceAddress(),
                    .triangleCount = uint32_t(softwareBlas->triangleBuffer.GetElementCount())
                });
                // Copied until a deformation was recorded, the update of a frame that isn't rendered is dropped
                frameDeformation.softwareBlases.push_back(SoftwareDeformBlas {
                    .blas = softwareBlas,
                    .levelNodeBuffer = deformMesh.levelNodeBuffer,
                    .nodeBoundsBuffer = instance.nodeBoundsBuffer,
                    .levelOffsets = deformMesh.levelOffsets,
                    .copySource = softwareBlas->IsBuilt() ? nullptr : instance.meshBlas
                });
            };

            auto isTraceable = [](const MeshComponent* meshComponent, const TransformComponent* transformComponent) {
                return meshComponent && transformComponent && meshComponent->visible && meshComponent->mesh.IsLoaded() &&
                    meshComponent->mesh->IsGeometryResident() && meshComponent->mesh->IsBVHBuilt() && meshComponent->mesh->rayTrace;
            };

            // An instance keeps its BLAS while the mesh, its BLAS and index buffer stay the same. Skinned meshes and soft
            // bodies also need a new one when switching between refits and builds
            auto getInstance = [&](ECS::Entity entity, const ResourceHandle<Mesh::Mesh>& mesh, bool dynamic) {
                auto item = deformedInstances.find(entity);
                auto created = item == deformedInstances.end() || item->second.mesh.GetID() != mesh.GetID() ||
                    item->second.meshBlas != mesh->blas || item->second.geometryVersion != mesh->GetGeometryVersion() ||
                    dynamic && hardwareRayTracing && item->second.blas->allowUpdate != refitSkinnedBlases;
                if (created)
                    item = deformedInstances.insert_or_assign(entity, CreateDeformedInstance(mesh)).first;
                return std::pair<DeformedInstance&, bool>(item->second, created);
            };

            // Skinned meshes and soft bodies near the camera are deformed with their joint matrices or simulated vertices,
            // which are appended to the source data of the frame. Poses that didn't change and asleep bodies keep their
            // BLAS and normals
            auto addDynamicInstance = [&](ECS::Entity entity, const MeshComponent& meshComponent, const TransformComponent& transformComponent,
                bool moved, auto& frameSourceData, const auto& sourceData, bool softBody) {
                // Entities which already have a BLAS keep it a little further out, so it isn't recreated at the border
                auto distance = deformedInstances.contains(entity) ? skinnedBlasDistance * 1.25f : skinnedBlasDistance;
                if (hasCamera && glm::distance2(vec3(transformComponent.globalMatrix[3]), cameraLocation) > distance * distance)
                    return;

                auto [instance, created] = getInstance(entity, meshComponent.mesh, true);
                instance.used = true;
                instance.active = true;
                instance.deformedStatic = false;
                if (!created && isBuilt(instance) && !moved)
                    return;

                auto sourceOffset = uint32_t(frameSourceData.size());
                frameSourceData.insert(frameSourceData.end(), sourceData.begin(), sourceData.end());
                addJobs(instance, transformComponent.globalMatrix, sourceOffset, created, softBody);
            };

            CPUProfiler::Scope profilerScope("Skinned instances");
            for (size_t i = 0; i < animationComponentPool.GetCount(); i++) {
                auto entity = animationComponentPool[i];
                const auto& animationComponent = animationComponentPool.GetByIndex(i);
                auto meshComponent = meshComponentPool.TryGet(entity);
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!isTraceable(meshComponent, transformComponent) || !meshComponent->mesh->data.IsSkinned() ||
                    animationComponent.jointMatrices.empty())
                    continue;

                addDynamicInstance(entity, *meshComponent, *transformComponent, animationComponent.IsPoseUpdated(),
                    jointMatrices, animationComponent.jointMatrices, false);
            }

            profilerScope.Next("Soft body instances");
            auto& softBodyComponentPool = scene->entityManager.GetPool<SoftBodyComponent>();
            for (size_t i = 0; i < softBodyComponentPool.GetCount(); i++) {
                auto entity = softBodyComponentPool[i];
                const auto& softBodyComponent = softBodyComponentPool.GetByIndex(i);
                auto meshComponent = meshComponentPool.TryGet(entity);
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!softBodyComponent.IsValid() || !isTraceable(meshComponent, transformComponent) ||
                    meshComponent->mesh->data.IsSkinned())
                    continue;

                const auto& meshData = meshComponent->mesh->data;
                if (softBodyComponent.vertices.size() != size_t(meshData.GetVertexCount() - meshData.lodVertexCount))
                    continue;

                addDynamicInstance(entity, *meshComponent, *transformComponent, softBodyComponent.HasMoved(),
                    softBodyVertices, softBodyComponent.vertices, true);
            }

            profilerScope.Next("Deformed instances");
            auto activeDistanceSqr = deformedBlasDistance * deformedBlasDistance;
            auto keepDistance = deformedBlasDistance * 1.25f;

            UpdateDeformCandidates(dirtyEntities, cameraLocation, hasCamera);

            for (auto entity : deformCandidates) {
                auto meshComponent = meshComponentPool.TryGet(entity);
                auto transformComponent = transformComponentPool.TryGet(entity);
                if (!isTraceable(meshComponent, transformComponent) || !deformedMeshes.contains(meshComponent->mesh.GetID()))
                    continue;

                // Has to be the same distance as the one of the GPU scene, which leaves these instances out
                auto distanceSqr = glm::distance2(vec3(transformComponent->globalMatrix[3]), cameraLocation);
                auto item = deformedInstances.find(entity);
                // Skinned instances and soft bodies were deformed above, simulated ones further away trace the mesh BLAS
                auto softBodyComponent = softBodyComponentPool.TryGet(entity);
                if ((item != deformedInstances.end() && item->second.used) || (softBodyComponent && softBodyComponent->IsValid()))
                    continue;
                if (distanceSqr > (item != deformedInstances.end() ? keepDistance * keepDistance : activeDistanceSqr))
                    continue;

                auto [instance, created] = getInstance(entity, meshComponent->mesh, false);
                instance.used = true;
                instance.deformedStatic = true;
                instance.active = distanceSqr < activeDistanceSqr;
                if (instance.active)
                    addJobs(instance, transformComponent->globalMatrix, 0, created, false);
            }

            profilerScope.Next("Deformation jobs");
            std::erase_if(deformedInstances, [](const auto& item) { return !item.second.used; });
            std::unordered_set<size_t> usedDeformMeshes;
            for (const auto& [entity, instance] : deformedInstances)
                usedDeformMeshes.insert(instance.mesh.GetID());
            std::erase_if(deformMeshes, [&](const auto& item) { return !usedDeformMeshes.contains(item.first); });

            // The software BLASes are bound like the ones of the meshes once they were deformed
            if (!hardwareRayTracing) {
                for (const auto& [entity, instance] : deformedInstances) {
                    auto bindlessIdx = renderState->blasToBindlessIdx.find(instance.softwareBlas);
                    if (bindlessIdx == renderState->blasToBindlessIdx.end())
                        continue;

                    auto [blasInfoIt, inserted] = blasInfos.try_emplace(instance.softwareBlas);
                    auto& blasInfo = blasInfoIt->second;
                    if (inserted) {
                        blasInfo.mesh = instance.mesh;
                        blasInfosChanged = true;
                    }
                    blasInfo.update = blasInfoUpdate;
                    blasInfo.offset = int32_t(bindlessIdx->second);
                    blasInfo.cullingDistanceSqr = instance.mesh->rayTraceDistanceCulling * instance.mesh->rayTraceDistanceCulling;
                }
            }

            if (positionCount == 0) {
                pendingDeformation = PendingDeformation();
                return;
            }

            auto positionSize = size_t(positionCount) * sizeof(vec3);
            auto normalOffset = hardwareRayTracing ? size_t(0) : (positionSize + 15) & ~size_t(15);
            auto vertexSize = hardwareRayTracing ? positionSize : normalOffset + size_t(positionCount) * sizeof(glm::uvec2);
            if (!deformVertexBuffer || deformVertexBuffer->size < vertexSize) {
                VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                if (hardwareRayTracing)
                    usageFlags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
                Graphics::BufferDesc desc = {
                    .usageFlags = usageFlags,
                    .domain = Graphics::BufferDomain::Device,
                    .size = vertexSize + vertexSize / 4
                };
                deformVertexBuffer = Graphics::GraphicsDevice::DefaultDevice->CreateBuffer(desc);
            }

            auto positionAddress = deformVertexBuffer->GetDeviceAddress();
            auto normalAddress = positionAddress + normalOffset;
            for (auto& deformBlas : frameDeformation.blases)
                deformBlas.vertexAddress += positionAddress;

            // Every work group looks up its job in the group table, so each pipeline is a single dispatch
            const uint32_t jobGroupSize = 64;
            std::vector<GPUDeformJob> jobs;
            std::vector<uint32_t> groupJobs;
            for (auto& [key, variant] : variantJobs) {
                auto groupOffset = uint32_t(groupJobs.size());
                for (auto& job : variant) {
                    job.positions += positionAddress;
                    if (!hardwareRayTracing)
                        job.normals += normalAddress;
                    job.firstGroup = uint32_t(groupJobs.size()) - groupOffset;
                    auto groupCount = (job.vertexCount + jobGroupSize - 1) / jobGroupSize;
                    groupJobs.insert(groupJobs.end(), groupCount, uint32_t(jobs.size()));
                    jobs.push_back(job);
                }
                frameDeformation.dispatches.push_back(DeformDispatch {
                    .config = variantConfigs[key],
                    .groupOffset = groupOffset,
                    .groupCount = uint32_t(groupJobs.size()) - groupOffset
                });
            }

            std::vector<uint32_t> triangleGroupJobs;
            for (auto& job : triangleJobs) {
                job.positions += positionAddress;
                job.normals += normalAddress;
                job.firstGroup = uint32_t(triangleGroupJobs.size());
                auto groupCount = (job.triangleCount + jobGroupSize - 1) / jobGroupSize;
                triangleGroupJobs.insert(triangleGroupJobs.end(), groupCount, uint32_t(&job - triangleJobs.data()));
            }

            frameDeformation.jobBuffer = uploadBufferPool.Upload(jobs.data(), jobs.size() * sizeof(GPUDeformJob));
            frameDeformation.groupJobBuffer = uploadBufferPool.Upload(groupJobs.data(), groupJobs.size() * sizeof(uint32_t));
            frameDeformation.jointMatrixBuffer = jointMatrices.empty() ? nullptr :
                uploadBufferPool.Upload(jointMatrices.data(), jointMatrices.size() * sizeof(mat3x4));
            frameDeformation.softBodyVertexBuffer = softBodyVertices.empty() ? nullptr :
                uploadBufferPool.Upload(softBodyVertices.data(), softBodyVertices.size() * sizeof(vec4));
            if (!triangleJobs.empty()) {
                frameDeformation.triangleJobBuffer = uploadBufferPool.Upload(triangleJobs.data(),
                    triangleJobs.size() * sizeof(GPUDeformTriangleJob));
                frameDeformation.triangleGroupJobBuffer = uploadBufferPool.Upload(triangleGroupJobs.data(),
                    triangleGroupJobs.size() * sizeof(uint32_t));
                frameDeformation.triangleGroupCount = uint32_t(triangleGroupJobs.size());
            }
            frameDeformation.frameIndex = Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex();

            pendingDeformation = std::move(frameDeformation);

        }

        void RayTracingWorld::UpdateDeformCandidates(const std::vector<ECS::Entity>& dirtyEntities, vec3 cameraLocation,
            bool hasCamera) {

            const auto& gpuScene = scene->renderState.gpuScene;
            auto& meshComponentPool = scene->entityManager.GetPool<MeshComponent>();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();
            auto keepDistance = deformedBlasDistance * 1.25f;

            // The deformed meshes and the octree query are expensive in large scenes, they are only searched again
            // once the camera moved a bit, the meshes of the GPU scene changed or after a while (compiled graphs join)
            auto candidateMargin = 0.1f * deformedBlasDistance;
            if (deformCandidateFrames++ >= 60 || deformCandidateSlotCount != gpuScene.slots.size() ||
                glm::distance2(cameraLocation, deformCandidateLocation) > candidateMargin * candidateMargin) {
                // Static meshes are only deformed near the camera with hardware ray tracing (software ray tracing would
                // copy the BVH per instance), the GPU scene writes all instances without one
                deformedMeshes.clear();
                for (const auto& slot : gpuScene.slots) {
                    if (hardwareRayTracing && hasCamera && !slot.instanceVertices && slot.mesh.IsLoaded() && slot.mesh->rayTrace &&
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

        }

        void RayTracingWorld::RecordDeformation(Graphics::CommandList* commandList) {

            auto& pending = pendingDeformation;
            if (pending.dispatches.empty() || pending.frameIndex != Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex())
                return;

            GPUProfiler::BeginQuery("Deform instances");

            // The buffers might still be read by the previous frame
            commandList->MemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT);

            // New software BLASes start with the triangles, materials and nodes of the mesh
            bool copied = false;
            for (const auto& softwareBlas : pending.softwareBlases) {
                if (!softwareBlas.copySource)
                    continue;
                const auto& source = softwareBlas.copySource;
                commandList->CopyBuffer(source->triangleBuffer.Get(), softwareBlas.blas->triangleBuffer.Get());
                commandList->CopyBuffer(source->bvhTriangleBuffer.Get(), softwareBlas.blas->bvhTriangleBuffer.Get());
                commandList->CopyBuffer(source->blasNodeBuffer.Get(), softwareBlas.blas->blasNodeBuffer.Get());
                copied = true;
            }
            if (copied)
                commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            commandList->BindBuffer(pending.jobBuffer, 3, 0);
            // Without skinned instances any buffer works
            commandList->BindBuffer(pending.jointMatrixBuffer ? pending.jointMatrixBuffer : pending.jobBuffer, 3, 1);
            commandList->BindBuffer(pending.groupJobBuffer, 3, 2);
            if (pending.softBodyVertexBuffer)
                commandList->BindBuffer(pending.softBodyVertexBuffer, 3, 4);
            scene->renderState.wind.noiseMap.Bind(commandList, 3, 3);
            scene->renderState.materialGraphBuffer.Bind(commandList, 3, 9);

            // The group count can exceed the limit of one dimension
            auto dispatchGroups = [&](uint32_t groupCount) {
                const uint32_t maxGroupsX = 32768;
                commandList->Dispatch(std::min(groupCount, maxGroupsX), (groupCount + maxGroupsX - 1) / maxGroupsX, 1);
            };

            for (auto& dispatch : pending.dispatches) {
                auto pipeline = PipelineManager::GetPipeline(dispatch.config);
                commandList->BindPipeline(pipeline);

                auto constants = JobPushConstants {
                    .groupOffset = dispatch.groupOffset,
                    .groupCount = dispatch.groupCount
                };
                commandList->PushConstants("constants", &constants);
                dispatchGroups(dispatch.groupCount);
            }

            if (!hardwareRayTracing) {
                commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                GPUProfiler::EndAndBeginQuery("Deform triangles");
                commandList->BindBuffer(pending.triangleJobBuffer, 3, 0);
                commandList->BindBuffer(pending.triangleGroupJobBuffer, 3, 2);
                commandList->BindPipeline(PipelineManager::GetPipeline(deformTrianglesPipelineConfig));
                auto triangleConstants = JobPushConstants {
                    .groupOffset = 0,
                    .groupCount = pending.triangleGroupCount
                };
                commandList->PushConstants("constants", &triangleConstants);
                dispatchGroups(pending.triangleGroupCount);

                commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                GPUProfiler::EndAndBeginQuery("Refit deformed BLASes");
                commandList->BindPipeline(PipelineManager::GetPipeline(refitBVHPipelineConfig));

                size_t levelCount = 0;
                for (const auto& softwareBlas : pending.softwareBlases)
                    levelCount = std::max(levelCount, softwareBlas.levelOffsets.size() - 1);

                // The levels of all BLASes are updated together, starting with the deepest one
                for (int32_t level = int32_t(levelCount) - 1; level >= 0; level--) {
                    for (const auto& softwareBlas : pending.softwareBlases) {
                        const auto& levelOffsets = softwareBlas.levelOffsets;
                        if (level + 1 >= int32_t(levelOffsets.size()))
                            continue;

                        softwareBlas.blas->blasNodeBuffer.Bind(commandList, 3, 0);
                        softwareBlas.blas->bvhTriangleBuffer.Bind(commandList, 3, 1);
                        softwareBlas.levelNodeBuffer.Bind(commandList, 3, 2);
                        softwareBlas.nodeBoundsBuffer.Bind(commandList, 3, 3);

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

                // Bound from the next update on, see SceneRenderState
                for (const auto& softwareBlas : pending.softwareBlases)
                    softwareBlas.blas->isBvhBuilt = true;
                pending = PendingDeformation();
                return;
            }

            // The positions are read by the BLAS builds, the normals by the hits
            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            std::vector<Ref<Graphics::BLAS>> buildBlases, refitBlases;
            for (const auto& deformBlas : pending.blases) {
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

            pending = PendingDeformation();

        }

        RayTracingWorld::DeformedInstance RayTracingWorld::CreateDeformedInstance(const ResourceHandle<Mesh::Mesh>& mesh) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            auto& data = mesh->data;
            // Only level 0 is deformed, the vertices of the other levels of detail follow its vertices
            auto vertexCount = size_t(data.GetVertexCount() - data.lodVertexCount);

            auto deformMeshItem = deformMeshes.find(mesh.GetID());
            if (deformMeshItem == deformMeshes.end() || deformMeshItem->second.meshBlas != mesh->blas ||
                deformMeshItem->second.geometryVersion != mesh->GetGeometryVersion()) {
                auto hasTangents = data.tangents.ContainsData();
                auto hasTexCoords = data.texCoords.ContainsData();
                auto hasTexCoords1 = data.texCoords1.ContainsData();
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
                    if (hasTexCoords1)
                        vertex.texCoord1 = glm::packHalf2x16(data.texCoords1[i]);
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
                deformMesh.geometryVersion = mesh->GetGeometryVersion();
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

                if (!hardwareRayTracing)
                    CreateSoftwareTopology(deformMesh, mesh);
            }

            if (!hardwareRayTracing) {
                const auto& meshBlas = mesh->blas;
                auto softwareBlas = CreateRef<BLAS>();
                softwareBlas->materials = meshBlas->materials;
                // Written by a copy of the mesh BLAS before the first deformation
                auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DeviceAddressBit;
                softwareBlas->triangleBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUTriangle),
                    meshBlas->triangleBuffer.GetElementCount());
                softwareBlas->bvhTriangleBuffer = Buffer::Buffer(bufferUsage, sizeof(GPUBVHTriangle),
                    meshBlas->bvhTriangleBuffer.GetElementCount());
                softwareBlas->blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(GPUBVHNode),
                    meshBlas->blasNodeBuffer.GetElementCount());

                const auto& levelOffsets = deformMeshes[mesh.GetID()].levelOffsets;
                return DeformedInstance {
                    .mesh = mesh,
                    .meshBlas = meshBlas,
                    .geometryVersion = mesh->GetGeometryVersion(),
                    .geometryRange = mesh->GetGeometryRange(),
                    .indexBuffer = mesh->indexBuffer,
                    .softwareBlas = softwareBlas,
                    .nodeBoundsBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, 2 * sizeof(vec4),
                        std::max(size_t(levelOffsets.back()), size_t(1)))
                };
            }

            std::vector<Graphics::ASGeometryRegion> geometryRegions;
            for (auto& subData : data.subData) {
                geometryRegions.emplace_back(Graphics::ASGeometryRegion{
                    .indexCount = subData.indicesCount,
                    .indexOffset = subData.indicesOffset,
                    .opaque = !subData.material.IsLoaded() || (!subData.material->opacityMap.IsValid() &&
                        subData.material->opacity == 1.0f && !subData.material->HasGraph() && !subData.material->IsTranslucent())
                    });
            }

            // The vertices are written into the position buffer of a frame before every build, see SetVertexData
            Graphics::ASBuilder asBuilder;
            auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(deformMeshes[mesh.GetID()].vertexBuffer.Get()->GetDeviceAddress(),
                mesh->indexBuffer.buffer->GetDeviceAddress() + mesh->indexBuffer.offset, vertexCount, sizeof(vec3),
                mesh->indexBuffer.elementSize, geometryRegions);
            blasDesc.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
            blasDesc.isDynamic = true;
            blasDesc.allowUpdate = refitSkinnedBlases;

            return DeformedInstance {
                .mesh = mesh,
                .meshBlas = mesh->blas,
                .blas = device->CreateBLAS(blasDesc),
                .normalBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DeviceAddressBit,
                    2 * sizeof(uint32_t), vertexCount),
                .geometryVersion = mesh->GetGeometryVersion(),
                .geometryRange = mesh->GetGeometryRange(),
                .indexBuffer = mesh->indexBuffer
            };

        }

        void RayTracingWorld::CreateSoftwareTopology(DeformMesh& deformMesh, const ResourceHandle<Mesh::Mesh>& mesh) {

            const auto& data = mesh->data;
            const auto& meshBlas = mesh->blas;

            // Parents always come before their children
            const auto& nodeParents = meshBlas->nodeParents;
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
            deformMesh.levelOffsets.clear();
            for (const auto& level : levels) {
                deformMesh.levelOffsets.push_back(uint32_t(levelNodes.size()));
                levelNodes.insert(levelNodes.end(), level.begin(), level.end());
            }
            deformMesh.levelOffsets.push_back(uint32_t(levelNodes.size()));

            deformMesh.levelNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(int32_t),
                std::max(levelNodes.size(), size_t(1)), levelNodes.empty() ? nullptr : levelNodes.data());

            // The source triangles of the BLAS are the ones of level 0 sub mesh by sub mesh, see MeshData::BuildBVHData
            std::vector<uint32_t> triangleIndexOffsets;
            for (const auto& subData : data.subData) {
                auto range = subData.GetLod(0);
                for (uint32_t i = 0; i < range.indicesCount / 3; i++)
                    triangleIndexOffsets.push_back(range.indicesOffset + i * 3);
            }

            const auto& triangleIndices = meshBlas->triangleIndices;
            std::vector<glm::uvec4> triangleVertices(triangleIndices.size());
            for (size_t i = 0; i < triangleIndices.size(); i++) {
                auto idx = triangleIndexOffsets[triangleIndices[i]];
                triangleVertices[i] = glm::uvec4(data.indices[idx], data.indices[idx + 1], data.indices[idx + 2], 0u);
            }

            deformMesh.triangleVertexBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                Buffer::BufferUsageBits::DeviceAddressBit, sizeof(glm::uvec4), triangleVertices.size(), triangleVertices.data());

        }

        bool RayTracingWorld::IsDeformedByMaterial(const ResourceHandle<Mesh::Mesh>& mesh) const {

            return std::any_of(mesh->data.subData.begin(), mesh->data.subData.end(), [](const auto& subData) {
                if (!subData.material.IsLoaded())
                    return false;

                const auto& graph = subData.material->graph;
                auto compiled = graph.IsLoaded() ? graph->GetCompiled() : nullptr;
                return compiled && compiled->valid && !compiled->deformFile.empty();
                });

        }

    }

}
