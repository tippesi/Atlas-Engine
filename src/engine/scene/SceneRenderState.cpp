#include "SceneRenderState.h"
#include "Scene.h"
#include "renderer/helper/CommonStructures.h"
#include "profiler/GPUProfiler.h"
#include "profiler/CPUProfiler.h"
#include "streaming/StreamingManager.h"

#include "common/ColorConverter.h"
#include "common/Packing.h"

#include "common/ColorConverter.h"
#include "common/MathHelper.h"
#include <glm/gtx/norm.hpp>

// Move most of the things in the main renderer, like the bindless update or the materials to here
// Also move rendering related map updates from the scene to here
namespace Atlas::Scene {

    SceneRenderState::SceneRenderState(Scene* scene) : scene(scene), gpuScene(scene) {

        materialBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
            Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit, sizeof(Renderer::PackedMaterial));
        // Ray tracing binds the graph parameters even if no graph has any
        materialGraphBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
            Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit, sizeof(uint32_t), 1);

    }

    SceneRenderState::~SceneRenderState() {
        
        WaitForAsyncWorkCompletion();
        
    }

    void SceneRenderState::NewFrame() {

        meshes = scene->GetMeshes();

        JobSystem::Execute(newFrameRenderListJob, [&](JobData&) {
            renderList.NewFrame(scene, meshes);

            for (auto& pass : renderList.passes) {
                pass->NewFrame(scene, meshes, renderList.meshIdToMeshMap);
            }
        });

    }

    void SceneRenderState::PrepareMaterials() {

        JobSystem::Wait(materialUpdateJob);

        JobSystem::Execute(materialUpdateJob, [&](JobData&) {
            auto sceneMaterials = scene->GetMaterials();

            // For debugging purpose
            if (scene->irradianceVolume && scene->irradianceVolume->debug) {
                const auto& internalVolume = scene->irradianceVolume->internal;
                sceneMaterials.push_back(internalVolume.probeDebugMaterial);
                sceneMaterials.push_back(internalVolume.probeDebugActiveMaterial);
                sceneMaterials.push_back(internalVolume.probeDebugInactiveMaterial);
                sceneMaterials.push_back(internalVolume.probeDebugOffsetMaterial);
            }

            uint16_t idx = 0;

            if (!materials.empty()) {
                materials.clear();
                uniqueMaterials.clear();
                materialMap.clear();
            }
            graphMaterials.clear();

            for (const auto& material : sceneMaterials) {
                // Might happen due to the scene giving back materials on a mesh basis
                if (materialMap.contains(material.Get().get()))
                    continue;

                uniqueMaterials.push_back(material.Get());

                auto usage = material->usage.load();

                if (material->graph.IsValid())
                    graphMaterials.push_back(material.Get());

                Renderer::PackedMaterial packed;

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(material->baseColor), 0.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(material->emissiveColor), 0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(material->transmissiveColor), 0.0f));

                packed.emissiveIntensityTiling = glm::packHalf2x16(vec2(material->emissiveIntensity, material->tiling));

                vec4 data0, data1, data2;

                data0.x = material->opacity;
                data0.y = material->roughness;
                data0.z = material->metalness;

                data1.x = material->ao;
                data1.y = material->HasNormalMap() ? material->normalScale : 0.0f;
                data1.z = material->HasDisplacementMap() ? material->displacementScale : 0.0f;

                data2.x = material->reflectance;
                // Note used
                data2.y = 0.0f;
                data2.z = 0.0f;

                packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
                packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
                packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

                packed.features = 0;

                packed.features |= material->HasBaseColorMap() ? Renderer::MaterialFeatures::FEATURE_BASE_COLOR_MAP : 0;
                packed.features |= material->HasOpacityMap() ? Renderer::MaterialFeatures::FEATURE_OPACITY_MAP : 0;
                packed.features |= material->HasNormalMap() ? Renderer::MaterialFeatures::FEATURE_NORMAL_MAP : 0;
                packed.features |= material->HasRoughnessMap() ? Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP : 0;
                packed.features |= material->HasMetalnessMap() ? Renderer::MaterialFeatures::FEATURE_METALNESS_MAP : 0;
                packed.features |= material->HasAoMap() ? Renderer::MaterialFeatures::FEATURE_AO_MAP : 0;
                packed.features |= material->HasEmissiveMap() ? Renderer::MaterialFeatures::FEATURE_EMISSIVE_MAP : 0;
                packed.features |= glm::length(material->transmissiveColor) > 0.0f ? Renderer::MaterialFeatures::FEATURE_TRANSMISSION : 0;
                packed.features |= material->vertexColors ? Renderer::MaterialFeatures::FEATURE_VERTEX_COLORS : 0;
                packed.features |= usage & MaterialUsageBits::TerrainBit ? Renderer::MaterialFeatures::FEATURE_TERRAIN : 0;

                materials.push_back(packed);

                materialMap[material.Get().get()] = idx++;
            }

            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded())
                    continue;

                auto& impostor = mesh->impostor;

                if (!impostor.IsLoaded())
                    continue;

                Renderer::PackedMaterial packed;

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(1.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(Common::ColorConverter::ConvertSRGBToLinear(impostor->approxTransmissiveColor), 1.0f));

                vec4 data0, data1, data2;

                data0.x = 1.0f;
                data0.y = 1.0f;
                data0.z = 1.0f;

                data1.x = 1.0f;
                data1.y = 0.0f;
                data1.z = 0.0f;

                data2.x = impostor->approxReflectance;
                // Note used
                data2.y = 0.0f;
                data2.z = 0.0f;

                packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
                packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
                packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

                packed.features = 0;

                packed.features |= Renderer::MaterialFeatures::FEATURE_BASE_COLOR_MAP |
                    Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP |
                    Renderer::MaterialFeatures::FEATURE_METALNESS_MAP |
                    Renderer::MaterialFeatures::FEATURE_AO_MAP;
                packed.features |= glm::length(impostor->approxTransmissiveColor) > 0.0f ?
                    Renderer::MaterialFeatures::FEATURE_TRANSMISSION : 0;

                materials.push_back(packed);

                materialMap[impostor.Get().get()] = idx++;
            }

            // The reflections start on the ocean surface, which is a perfect mirror for them
            if (scene->ocean) {
                Renderer::PackedMaterial packed;

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));
                packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));

                packed.emissiveIntensityTiling = glm::packHalf2x16(vec2(0.0f, 1.0f));

                vec4 data0, data1, data2;

                data0.x = 1.0f;
                data0.y = 0.0f;
                data0.z = 0.0f;

                data1.x = 1.0f;
                data1.y = 0.0f;
                data1.z = 0.0f;

                // F0 of water is 0.02
                data2.x = sqrtf(0.02f / 0.16f);
                data2.y = 0.0f;
                data2.z = 0.0f;

                packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
                packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
                packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

                packed.features = 0;

                materials.push_back(packed);

                materialMap[scene->ocean.get()] = idx++;
            }

            // The fragments of a graph write all final values into the G-buffer, so these entries don't change them
            // The properties of the material stay for the regular entry, which the fallback pipeline relies on
            materialGraphMap.clear();
            for (const auto& material : graphMaterials) {
                if (!material->HasGraph())
                    continue;

                auto compiled = material->graph->GetCompiled();
                if (!compiled || !compiled->valid)
                    continue;

                auto& data = materialGraphMap[material.get()];
                data.materialIdx = uint32_t(materialMap[material.get()]);
                if (!compiled->hasSurface)
                    continue;

                auto packed = materials[data.materialIdx];

                packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(1.0f));
                packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(1.0f));
                packed.emissiveIntensityTiling = glm::packHalf2x16(vec2(1.0f, material->tiling));

                vec4 data0(material->opacity, 1.0f, 1.0f, 0.0f);
                packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
                vec4 data1(1.0f, 1.0f, 0.0f, 0.0f);
                packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);

                packed.features |= Renderer::MaterialFeatures::FEATURE_BASE_COLOR_MAP | Renderer::MaterialFeatures::FEATURE_NORMAL_MAP |
                    Renderer::MaterialFeatures::FEATURE_ROUGHNESS_MAP | Renderer::MaterialFeatures::FEATURE_METALNESS_MAP |
                    Renderer::MaterialFeatures::FEATURE_AO_MAP | Renderer::MaterialFeatures::FEATURE_EMISSIVE_MAP;

                materials.push_back(packed);
                data.materialIdx = uint32_t(idx++);
            }

            if (materials.size() > materialBuffer.GetElementCount()) {
                materialBuffer.SetSize(materials.size());
            }

            if (!materials.empty())
                materialBuffer.SetData(materials.data(), 0, materials.size());

            UpdateTextureBindlessData();
            });

    }

    void SceneRenderState::UpdateMaterialGraphParameters(
        const std::function<uint32_t(const ResourceHandle<Texture::Texture2D>&, MaterialGraph::DefaultTexture)>& requestTexture) {

        materialGraphParameters.clear();

        for (const auto& material : graphMaterials) {
            auto dataIter = materialGraphMap.find(material.get());
            if (dataIter == materialGraphMap.end())
                continue;

            // Looking up the values by their names and converting colors is too much for every frame
            auto compiled = material->graph->GetCompiled();
            auto& cache = materialGraphCaches[material.get()];
            if (cache.compiled != compiled || cache.material != material || cache.version != material->graphParameterVersion) {
                cache.material = material;
                cache.compiled = compiled;
                cache.version = material->graphParameterVersion;
                cache.words.assign(compiled->parameterWords, 0u);
                cache.textures.clear();

                for (const auto& parameter : compiled->parameters) {
                    if (parameter.type == MaterialGraph::ValueType::Texture)
                        cache.textures.push_back({ std::addressof(material->GetGraphTexture(parameter)), parameter.offset, parameter.fallback });
                    else
                        parameter.WriteValue(material->GetGraphValue(parameter), cache.words.data() + parameter.offset);
                }
            }
            cache.lastUsedFrame = textureFrameCount;

            auto offset = uint32_t(materialGraphParameters.size());
            dataIter->second.parameterOffset = offset;
            materialGraphParameters.insert(materialGraphParameters.end(), cache.words.begin(), cache.words.end());

            // The slots of textures change between frames
            for (const auto& texture : cache.textures)
                materialGraphParameters[offset + texture.offset] = requestTexture(*texture.handle, texture.fallback);
        }

        if (materialGraphCaches.size() > graphMaterials.size())
            std::erase_if(materialGraphCaches, [&](const auto& item) { return item.second.lastUsedFrame != textureFrameCount; });

        if (materialGraphParameters.empty())
            return;

        if (materialGraphParameters.size() > materialGraphBuffer.GetElementCount())
            materialGraphBuffer.SetSize(materialGraphParameters.size());

        materialGraphBuffer.SetData(materialGraphParameters.data(), 0, materialGraphParameters.size());

    }

    void SceneRenderState::UpdateBlasBindlessData(bool rayTracing) {

        // The material update reads the terrain leaf nodes
        JobSystem::Wait(materialUpdateJob);

        terrainLeafNodes.clear();
        if (scene->terrain.IsLoaded()) {
            auto& leafNodes = scene->terrain->leafList;
            terrainLeafNodes.reserve(leafNodes.size());
            std::copy(leafNodes.begin(), leafNodes.end(), std::back_inserter(terrainLeafNodes));
        }

        JobSystem::Wait(bindlessBlasMapUpdateJob);
        JobSystem::Wait(prepareBindlessBlasesJob);

        // Nothing reads these buffers without any rt effects running
        if (!rayTracing) {
            blasToBindlessIdx.clear();
            blasBuffers.clear();
            triangleBuffers.clear();
            bvhTriangleBuffers.clear();
            triangleOffsetBuffers.clear();
            return;
        }

        auto bindlessBlasBuffersUpdate = [&](JobData&) {
            JobSystem::Wait(bindlessBlasMapUpdateJob);

            if (blasBuffers.size() != blasToBindlessIdx.size()) {
                blasBuffers.resize(blasToBindlessIdx.size());
                triangleBuffers.resize(blasToBindlessIdx.size());
                bvhTriangleBuffers.resize(blasToBindlessIdx.size());
                triangleOffsetBuffers.resize(blasToBindlessIdx.size());
            }

            for (const auto& [blas, idx] : blasToBindlessIdx) {
                auto blasBuffer = blas->blasNodeBuffer.Get();
                auto triangleBuffer = blas->triangleBuffer.Get();
                auto bvhTriangleBuffer = blas->bvhTriangleBuffer.Get();
                auto triangleOffsetBuffer = blas->triangleOffsetBuffer.Get();

                AE_ASSERT(triangleBuffer != nullptr);

                blasBuffers[idx] = blasBuffer;
                triangleBuffers[idx] = triangleBuffer;
                bvhTriangleBuffers[idx] = bvhTriangleBuffer;
                triangleOffsetBuffers[idx] = triangleOffsetBuffer;
            }
            };

        auto bindlessBlasMapUpdate = [&, bindlessBlasBuffersUpdate](JobData&) {

            blasToBindlessIdx.clear();

            uint32_t bufferIdx = 0;
            for (const auto& mesh : meshes) {
                if (!mesh.IsLoaded()) continue;

                // Not all meshes might have a bvh and not all blases will be built in frame, so skip them if they are not ready
                if (!mesh->IsBVHBuilt() || mesh->IsBVHBuilt() && mesh->blas->blas && 
                    !mesh->blas->blas->isDynamic && !mesh->blas->blas->isBuilt)
                    continue;

                blasToBindlessIdx[mesh->blas] = bufferIdx++;
            }

            if (scene->rayTracingWorld) {
                for (const auto& [entity, skinnedBlas] : scene->rayTracingWorld->skinnedBlases) {
                    if (skinnedBlas.blas->IsBuilt())
                        blasToBindlessIdx[skinnedBlas.blas] = bufferIdx++;
                }
            }

            for (const auto leafNode : terrainLeafNodes) {
                auto leafCell = leafNode->cell;
                if (!leafCell || !leafCell->IsLoaded() || !leafCell->blas || !leafCell->blas->IsBuilt())
                    continue;

                blasToBindlessIdx[leafCell->blas] = bufferIdx++;
            }
            };

        JobSystem::Execute(bindlessBlasMapUpdateJob, bindlessBlasMapUpdate);
        JobSystem::Execute(prepareBindlessBlasesJob, bindlessBlasBuffersUpdate);

    }

    void SceneRenderState::UpdateTextureBindlessData() {

        if (textureSlots.empty())
            CreateDefaultBindlessTextures();

        textureFrameCount++;
        newTextureSlotCount = 0;

        auto requestTexture = [&](const ResourceHandle<Texture::Texture2D>& texture, DefaultTextureSlot defaultSlot) {
            return texture.IsLoaded() ? RequestBindlessTexture(texture.Get(), defaultSlot) : uint32_t(defaultSlot);
            };

        materialTextureIndices.resize(uniqueMaterials.size());
        for (size_t i = 0; i < uniqueMaterials.size(); i++) {
            const auto& material = uniqueMaterials[i];
            auto& indices = materialTextureIndices[i];

            indices.baseColor = requestTexture(material->baseColorMap, DefaultTextureSlot::White);
            indices.opacity = requestTexture(material->opacityMap, DefaultTextureSlot::White);
            indices.normal = requestTexture(material->normalMap, DefaultTextureSlot::Normal);
            indices.roughness = requestTexture(material->roughnessMap, DefaultTextureSlot::White);
            indices.metalness = requestTexture(material->metalnessMap, DefaultTextureSlot::Black);
            indices.ao = requestTexture(material->aoMap, DefaultTextureSlot::White);
            indices.height = requestTexture(material->displacementMap, DefaultTextureSlot::Black);
            indices.emissive = requestTexture(material->emissiveMap, DefaultTextureSlot::Black);
        }

        if (!materialGraphMap.empty()) {
            UpdateMaterialGraphParameters([&](const ResourceHandle<Texture::Texture2D>& texture, MaterialGraph::DefaultTexture fallback) {
                auto defaultSlot = fallback == MaterialGraph::DefaultTexture::Normal ? DefaultTextureSlot::Normal :
                    fallback == MaterialGraph::DefaultTexture::Black ? DefaultTextureSlot::Black : DefaultTextureSlot::White;
                return requestTexture(texture, defaultSlot);
                });
        }

        for (const auto leafNode : terrainLeafNodes) {
            auto leafCell = leafNode->cell;
            if (!leafCell || !leafCell->IsLoaded() || !leafCell->blas || !leafCell->blas->IsBuilt())
                continue;

            if (leafCell->normalMap)
                RequestBindlessTexture(leafCell->normalMap, DefaultTextureSlot::Normal);
        }

        EvictUnusedBindlessTextures();

    }

    uint32_t SceneRenderState::GetBindlessTextureIdx(const Ref<Texture::Texture2D>& texture) const {

        auto it = textureToBindlessIdx.find(texture);
        return it != textureToBindlessIdx.end() ? it->second : uint32_t(DefaultTextureSlot::White);

    }

    uint32_t SceneRenderState::RequestBindlessTexture(const Ref<Texture::Texture2D>& texture, DefaultTextureSlot defaultSlot) {

        auto it = textureToBindlessIdx.find(texture);
        if (it != textureToBindlessIdx.end()) {
            auto slotIdx = it->second;
            textureSlots[slotIdx].lastUsedFrame = textureFrameCount;
            if (textures[slotIdx] != texture->image)
                textures[slotIdx] = texture->image;
            return slotIdx;
        }

        // Slots which can't be assigned this frame use a default until the next frames
        if (newTextureSlotCount >= maxNewTextureSlotsPerFrame)
            return uint32_t(defaultSlot);

        uint32_t slotIdx;
        if (!freeTextureSlots.empty()) {
            slotIdx = freeTextureSlots.back();
            freeTextureSlots.pop_back();
        }
        else if (textureSlots.size() < BINDLESS_TEXTURE_COUNT) {
            slotIdx = uint32_t(textureSlots.size());
            textureSlots.emplace_back();
            textures.emplace_back();
        }
        else {
            return uint32_t(defaultSlot);
        }

        textureSlots[slotIdx] = { texture, textureFrameCount };
        textures[slotIdx] = texture->image;
        textureToBindlessIdx[texture] = slotIdx;
        newTextureSlotCount++;

        return slotIdx;

    }

    void SceneRenderState::CreateDefaultBindlessTextures() {

        std::vector<std::vector<uint8_t>> defaultData = {
            { 255, 255, 255, 255 },
            { 0, 0, 0, 255 },
            { 128, 128, 255, 255 }
        };

        for (auto& data : defaultData) {
            auto texture = CreateRef<Texture::Texture2D>(1, 1, VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::Repeat, Texture::Filtering::Nearest);
            texture->SetData(data);

            RequestBindlessTexture(texture, DefaultTextureSlot::White);
        }

    }

    void SceneRenderState::EvictUnusedBindlessTextures() {

        auto slotCount = uint32_t(textureSlots.size());
        auto checkCount = std::min(slotCount, textureEvictionChecksPerFrame);

        for (uint32_t i = 0; i < checkCount; i++) {
            textureEvictionIdx = (textureEvictionIdx + 1) % slotCount;
            auto& slot = textureSlots[textureEvictionIdx];

            if (textureEvictionIdx < uint32_t(DefaultTextureSlot::Count) || !slot.texture ||
                textureFrameCount - slot.lastUsedFrame < textureEvictionFrameCount)
                continue;

            textureToBindlessIdx.erase(slot.texture);
            slot.texture.reset();
            textures[textureEvictionIdx].reset();
            freeTextureSlots.push_back(textureEvictionIdx);
        }

    }

    void SceneRenderState::RequestStreamedResources() {

        CPUProfiler::Scope profilerScope("Request streamed resources");

        // The last distances are requested again until the GPU delivers new ones, without new ones (e.g. while the
        // scene isn't rendered) nothing is unloaded
        gpuScene.TakeMeshDistances(meshDistances);
        auto focalLength = gpuScene.GetFocalLength();
        if (focalLength <= 0.0f)
            return;

        // Materials are shared by many meshes, each one is requested once with its largest resolution
        materialResolutions.clear();
        auto requestMaterials = [&](const ResourceHandle<Mesh::Mesh>& mesh, float distance) {
            for (const auto& subData : mesh->data.subData) {
                if (!subData.material.IsLoaded())
                    continue;

                auto material = subData.material.Get().get();
                auto resolution = Streaming::TextureStreaming::GetResolution(subData.uvScale / material->tiling,
                    distance, focalLength);
                auto& materialResolution = materialResolutions[material];
                materialResolution = glm::max(materialResolution, resolution);
            }
            };

        // Each update requests a part of the meshes in rotation, reductions wait for many updates anyway
        meshRequests.clear();
        impostorRequests.clear();
        const auto& meshSettings = Streaming::StreamingManager::settings.meshes;
        const auto& slots = gpuScene.slots;
        auto slotCount = glm::min(meshDistances.meshIds.size(), slots.size());
        const size_t requestRotation = Streaming::Residency::requestRotation;
        streamingRequestOffset = (streamingRequestOffset + 1) % requestRotation;
        for (size_t i = streamingRequestOffset; i < slotCount; i += requestRotation) {
            const auto& slot = slots[i];
            if (meshDistances.meshIds[i] == 0 || slot.mesh.GetID() != meshDistances.meshIds[i] || !slot.mesh.IsLoaded())
                continue;

            // Meshes without an instance that needs them keep the largest distance, a NaN. They are requested
            // anyway, so their impostors are unloaded as well
            auto positionDistance = meshDistances.positionDistances[i];
            meshRequests.push_back({ std::addressof(slot.mesh),
                std::isfinite(positionDistance) ? positionDistance : std::numeric_limits<float>::max() });

            auto distance = meshDistances.distances[i];
            if (std::isfinite(distance))
                requestMaterials(slot.mesh, distance);

            // Impostors are drawn beyond their distances, meshes without a needed instance don't request theirs
            const auto& mesh = slot.mesh;
            if (std::isfinite(positionDistance) && mesh->impostor.IsLoaded() && mesh->impostor->isGenerated) {
                auto cullingDistance = glm::max(mesh->distanceCulling, mesh->castShadow ? mesh->shadowDistanceCulling : 0.0f);
                auto impostorDistance = mesh->castShadow ? glm::min(mesh->impostorDistance, mesh->impostorShadowDistance) :
                    mesh->impostorDistance;
                if (positionDistance < cullingDistance + meshSettings.distanceMargin) {
                    auto resolution = Streaming::ImpostorStreaming::GetResolution(mesh->impostor->radius,
                        glm::max(positionDistance, impostorDistance), focalLength);
                    impostorRequests.push_back({ mesh->impostor.Get().get(), resolution });
                }
            }
        }

        // Clutter is drawn around the camera and isn't culled on the GPU, it needs its meshes in full detail
        std::vector<ResourceHandle<Mesh::Mesh>> clutterMeshes;
        if (scene->clutter)
            clutterMeshes = scene->clutter->GetMeshes();
        for (const auto& mesh : clutterMeshes) {
            if (!mesh.IsLoaded())
                continue;
            meshRequests.push_back({ std::addressof(mesh), 0.0f });
            requestMaterials(mesh, 0.0f);
        }

        // Skinned meshes aren't culled on the GPU
        if (scene->HasMainCamera()) {
            auto cameraLocation = scene->GetMainCamera().GetLocation();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();
            for (const auto& slot : slots) {
                if (!slot.skinned || !slot.mesh.IsLoaded())
                    continue;

                auto minDistance = std::numeric_limits<float>::max();
                for (auto entity : slot.entities) {
                    // The slots are cleaned up later in the update, entities might be gone since the last one
                    auto transformComponent = transformComponentPool.TryGet(entity);
                    if (!transformComponent)
                        continue;

                    const auto& matrix = transformComponent->globalMatrix;
                    auto scale = glm::max(glm::length(vec3(matrix[0])),
                        glm::max(glm::length(vec3(matrix[1])), glm::length(vec3(matrix[2]))));
                    auto distance = slot.mesh->data.aabb.Transform(matrix).GetDistance(cameraLocation);
                    minDistance = glm::min(minDistance, distance / scale);
                }
                requestMaterials(slot.mesh, minDistance);
            }

            // The terrain samples its materials at the position times their tiling, the distance is the height
            // of the camera above it
            if (scene->terrain.IsLoaded() && scene->terrain->storage) {
                const auto& terrain = scene->terrain;
                auto height = terrain->GetHeight(cameraLocation.x, cameraLocation.z);
                auto bounds = Volume::AABB(terrain->translation,
                    terrain->translation + vec3(terrain->sideLength, terrain->heightScale, terrain->sideLength));
                auto distance = height != Terrain::Terrain::invalidHeight ?
                    glm::abs(cameraLocation.y - height) : bounds.GetDistance(cameraLocation);

                for (const auto& material : terrain->storage->materials) {
                    if (!material.IsLoaded())
                        continue;
                    auto resolution = Streaming::TextureStreaming::GetResolution(1.0f / material->tiling,
                        distance, focalLength);
                    auto& materialResolution = materialResolutions[material.Get().get()];
                    materialResolution = glm::max(materialResolution, resolution);
                }
            }
        }

        materialRequests.clear();
        for (const auto& [material, resolution] : materialResolutions)
            materialRequests.push_back({ material, resolution });

        Streaming::MeshStreaming::Request(meshRequests, scene->IsRtDataNeeded());
        Streaming::TextureStreaming::Request(materialRequests);
        Streaming::ImpostorStreaming::Request(impostorRequests);

    }

    void SceneRenderState::UpdateOtherTextureBindlessData() {

        auto lightSubset = scene->GetSubset<LightComponent>();
        JobSystem::Wait(bindlessOtherTextureMapUpdateJob);

        JobSystem::Execute(bindlessOtherTextureMapUpdateJob, [&, lightSubset](JobData&) {

            cubemapToBindlessIdx.clear();
            textureArrayToBindlessIdx.clear();

            cubemaps.clear();
            textureArrays.clear();

            uint32_t textureArrayIdx = 0;
            uint32_t cubemapIdx = 0;
            for (auto entity : lightSubset) {
                const auto& lightComponent = entity.GetComponent<LightComponent>();

                if (!lightComponent.shadow)
                    continue;

                if (lightComponent.shadow->useCubemap) {
                    cubemapToBindlessIdx[lightComponent.shadow->cubemap] = cubemapIdx++;
                    cubemaps.push_back(lightComponent.shadow->cubemap->image);
                }
                else {
                    textureArrayToBindlessIdx[lightComponent.shadow->maps] = textureArrayIdx++;
                    textureArrays.push_back(lightComponent.shadow->maps->image);
                }
            }
            });

    }

    void SceneRenderState::UpdateGPUScene(const std::vector<ECS::Entity>& dirtyEntities,
        const std::vector<ECS::Entity>& lastDirtyEntities) {

        // The buffers are updated in the same job, everything which binds them waits for it
        JobSystem::Execute(gpuSceneUpdateJob, [&](JobData&) {
            auto device = Graphics::GraphicsDevice::DefaultDevice;
            // The upload buffers belong to the frame which is recorded next
            device->WaitForPreviousFrameSubmission();
            gpuScene.Update(dirtyEntities, lastDirtyEntities);

            // The changes of this frame are lost, everything is uploaded again
            if (!device->swapChain->isComplete) {
                gpuScene.Invalidate();
                return;
            }

            auto commandList = device->GetCommandList();
            commandList->BeginCommands();
            GPUProfiler::BeginThread("GPU scene", commandList);

            gpuScene.UpdateBuffers(commandList);

            GPUProfiler::EndThread();
            commandList->EndCommands();
            device->SubmitCommandList(commandList);
            });

    }

    void SceneRenderState::FillMainRenderPass() {

        if (!scene->HasMainCamera())
            return;

        JobSystem::Execute(fillMainRenderPassJob, [&](JobData&) {
            JobSystem::Wait(newFrameRenderListJob);

            auto& camera = scene->GetMainCamera();

            auto mainPass = renderList.GetMainPass();
            if (mainPass == nullptr) {
                mainPass = renderList.NewMainPass();
                // Usually this gets done by the newFrameRenderListJob for existing passes
                mainPass->NewFrame(scene, meshes, renderList.meshIdToMeshMap);
            }

            JobSystem::Wait(gpuSceneUpdateJob);
            gpuScene.FillPass(camera.frustum, camera.GetLocation(), mainPass);
            renderList.AddGPUCulledPass(mainPass);
            mainPass->Update(camera.GetLocation(), renderList.meshIdToMeshMap);
            mainPass->FillBuffers();
            renderList.FinishPass(mainPass, RenderList::RenderPassType::Main);
        });

    }

    void SceneRenderState::FillShadowRenderPass(Entity entity) {

        if (!scene->HasMainCamera())
            return;

        // There reference will stay intact even after the method is exited, the source should be immutable until 
        // the current frame has ended...
        auto& light = entity.GetComponent<LightComponent>();
        if (!light.shadow || !light.shadow->update)
            return;

        // The cascades of directional lights cover most of the scene, other lights stay culled on the host.
        // The renderer waits for the GPU culled passes separately.
        auto gpuCulled = light.type == LightType::DirectionalLight;
        auto jobGroup = gpuCulled ? &fillGPUShadowRenderPassesJob : &fillShadowRenderPassesJob;

        JobSystem::Execute(*jobGroup, [&, entity = entity, gpuCulled, jobGroup](JobData&) {
            {
                CPUProfiler::Scope waitScope("Wait for the render list");
                JobSystem::Wait(newFrameRenderListJob);
                if (gpuCulled)
                    JobSystem::Wait(gpuSceneUpdateJob);
            }

            auto& camera = scene->GetMainCamera();
            auto& shadow = light.shadow;

            auto componentCount = shadow->longRange ?
                shadow->viewCount - 1 : shadow->viewCount;

            JobSystem::ExecuteMultiple(*jobGroup, componentCount,
                [&, shadow = shadow, entity = entity, gpuCulled](JobData& data) {
                    auto component = &shadow->views[data.idx];
                    auto frustum = Volume::Frustum(component->frustumMatrix);

                    auto shadowPass = renderList.GetShadowPass(entity, data.idx);
                    if (shadowPass == nullptr) {
                        shadowPass = renderList.NewShadowPass(entity, data.idx);
                        // Usually this gets done by the newFrameRenderListJob for existing passes
                        shadowPass->NewFrame(scene, meshes, renderList.meshIdToMeshMap);
                    }

                    CPUProfiler::Scope profilerScope("Shadow pass culling");
                    if (gpuCulled) {
                        gpuScene.FillPass(frustum, camera.GetLocation(), shadowPass);
                        renderList.AddGPUCulledPass(shadowPass);
                    }
                    else {
                        scene->GetRenderList(frustum, shadowPass);
                    }
                    profilerScope.Next("Shadow pass update");
                    shadowPass->Update(camera.GetLocation(), renderList.meshIdToMeshMap);
                    profilerScope.Next("Shadow pass buffers");
                    shadowPass->FillBuffers();
                    renderList.FinishPass(shadowPass, RenderList::RenderPassType::Shadow);
                });
        });

    }

    void SceneRenderState::CullAndSortLights() {

        JobSystem::Wait(cullAndSortLightsJob);

        if (!scene->HasMainCamera())
            return;

        auto& camera = scene->GetMainCamera();
        auto cameraFrustum = camera.frustum;

        JobSystem::Execute(cullAndSortLightsJob, [&, camera, cameraFrustum](JobData&) {

            lightEntities.clear();
            lightEntities.reserve(scene->GetComponentCount<LightComponent>());
            auto lightSubset = scene->GetSubset<LightComponent>();
            for (auto& lightEntity : lightSubset) {
                auto& light = lightEntity.GetComponent<LightComponent>();
                if (!light.IsVisible(cameraFrustum))
                    continue;
                lightEntities.emplace_back(LightEntity{ lightEntity, light });
            }

            if (lightEntities.size() > 1) {
                auto cameraLocation = camera.GetLocation();
                auto getPositionForLight = [cameraLocation](const LightEntity& light) -> vec3 {
                    switch (light.comp.type) {
                    case LightType::PointLight: return light.comp.transformedProperties.point.position;
                    case LightType::SpotLight: return light.comp.transformedProperties.spot.position;
                    default: return cameraLocation;
                    }
                    };

                std::sort(lightEntities.begin(), lightEntities.end(),
                    [&](const LightEntity& light0, const LightEntity& light1) {
                        if (light1.comp.isMain)
                            return false;
                        if (light0.comp.isMain)
                            return true;
                        if (light0.comp.type == LightType::DirectionalLight)
                            return true;

                        return glm::distance2(getPositionForLight(light0), cameraLocation)
                            < glm::distance2(getPositionForLight(light1), cameraLocation);
                    });
            }

            JobSystem::Wait(bindlessOtherTextureMapUpdateJob);

            // Always clear lights to get rid of old state
            lights.clear();
            volumetricLights.clear();
            volumetricShadows.clear();

            if (lightEntities.size()) {

                for (const auto& entity : lightEntities) {
                    auto& light = entity.comp;

                    auto type = static_cast<uint32_t>(light.type);
                    auto packedType = reinterpret_cast<float&>(type);
                    Renderer::Light lightUniform {
                        .color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), packedType),
                        .intensity = light.intensity,
                        .scatteringFactor = 1.0f,
                    };

                    const auto& prop = light.transformedProperties;
                    if (light.type == LightType::DirectionalLight) {
                        lightUniform.direction = camera.viewMatrix * vec4(prop.directional.direction, 0.0f);
                        auto transmittance = scene->sky.GetDirectLightTransmittance(camera.GetLocation(), prop.directional.direction);
                        lightUniform.color = vec4(vec3(lightUniform.color) * transmittance, packedType);
                    }
                    else if (light.type == LightType::PointLight) {
                        lightUniform.location = camera.viewMatrix * vec4(prop.point.position, 1.0f);
                        lightUniform.direction.w = prop.point.radius;
                    }
                    else if (light.type == LightType::SpotLight) {
                        lightUniform.location = camera.viewMatrix * vec4(prop.spot.position, 1.0f);
                        lightUniform.direction = camera.viewMatrix * vec4(prop.spot.direction, 0.0f);
                        lightUniform.direction.w = prop.spot.radius;
                        
                        auto tanOuter = tanf(prop.spot.outerConeAngle);
                        auto cosOuter = cosf(prop.spot.outerConeAngle);
                        auto cosInner = cosf(prop.spot.innerConeAngle);
                        auto angleScale = 1.0f / std::max(0.001f, cosInner - cosOuter);
                        auto angleOffset = -cosOuter * angleScale;

                        lightUniform.typeSpecific0 = angleScale;
                        lightUniform.typeSpecific1 = angleOffset;

                        uint32_t coneTrig = glm::packHalf2x16(vec2(cosOuter, tanOuter));
                        lightUniform.location.w = reinterpret_cast<float&>(coneTrig);
                    }

                    if (light.shadow) {
                        auto shadow = light.shadow;
                        auto& shadowUniform = lightUniform.shadow;
                        shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                        shadowUniform.bias = shadow->bias;
                        shadowUniform.edgeSoftness = shadow->edgeSoftness;
                        shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                        shadowUniform.cascadeCount = shadow->viewCount;
                        shadowUniform.resolution = vec2(shadow->resolution);
                        shadowUniform.mapIdx = shadow->useCubemap ? cubemapToBindlessIdx[shadow->cubemap] : textureArrayToBindlessIdx[shadow->maps];

                        auto componentCount = shadow->viewCount;
                        for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                            if (i < componentCount) {
                                auto cascade = &shadow->views[i];
                                auto frustum = Volume::Frustum(cascade->frustumMatrix);
                                auto corners = frustum.GetCorners();
                                auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                                    abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                                if (light.type == LightType::DirectionalLight) {
                                    auto matrix = cascade->projectionMatrix *
                                        cascade->viewMatrix * camera.invViewMatrix;
                                    shadowUniform.cascades[i].cascadeSpace = glm::transpose(matrix);
                                }
                                shadowUniform.cascades[i].texelSize = texelSize;
                            }
                            else {
                                auto cascade = &shadow->views[componentCount - 1];
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                            }
                        }

                        if (light.type == LightType::PointLight) {
                            auto projectionMatrix = shadow->views[0].projectionMatrix;
                            shadowUniform.cascades[0].cascadeSpace = glm::transpose(glm::translate(mat4(1.0f), -prop.point.position) * camera.invViewMatrix);
                            shadowUniform.cascades[1].cascadeSpace[0] = projectionMatrix[0];
                            shadowUniform.cascades[1].cascadeSpace[1] = projectionMatrix[1];
                            shadowUniform.cascades[1].cascadeSpace[2] = projectionMatrix[2];
                            shadowUniform.cascades[2].cascadeSpace[0] = projectionMatrix[3];
                        }
                        else if (light.type == LightType::SpotLight) {
                            auto cascadeMatrix = shadow->views[0].projectionMatrix *
                                shadow->views[0].viewMatrix * camera.invViewMatrix;
                            shadowUniform.cascades[0].cascadeSpace[0] = cascadeMatrix[0];
                            shadowUniform.cascades[0].cascadeSpace[1] = cascadeMatrix[1];
                            shadowUniform.cascades[0].cascadeSpace[2] = cascadeMatrix[2];
                            shadowUniform.cascades[1].cascadeSpace[0] = cascadeMatrix[3];
                        }
                    }
                    else {
                        lightUniform.shadow.mapIdx = -1;
                    }

                    lights.push_back(lightUniform);

                    if (light.volumetricIntensity > 0.0f) {
                        Renderer::VolumetricLight volumetricLightUniform{
                            .location = lightUniform.location,
                            .direction = lightUniform.direction,
                            .color = lightUniform.color,
                            .intensity = light.volumetricIntensity * light.intensity,
                            .typeSpecific0 = lightUniform.typeSpecific0,
                            .typeSpecific1 = lightUniform.typeSpecific1,
                            .shadowIdx = light.shadow ? int32_t(volumetricShadows.size()) : -1
                        };

                        volumetricLights.push_back(volumetricLightUniform);
                        volumetricShadows.push_back(lightUniform.shadow);
                    }
                }
            }
            else {
                // We need to have at least a fake light
                auto type = 0;
                auto packedType = reinterpret_cast<float&>(type);
                lights.emplace_back(Renderer::Light {
                        .direction = vec4(0.0f, -1.0f, 0.0f, 0.0f),
                        .color = vec4(vec3(0.0f), packedType),
                        .intensity = 0.0f,
                    });
            }

            if (lightBuffer.GetElementCount() < lights.size()) {
                lightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::Light), lights.size(), lights.data());
            }
            else {
                lightBuffer.SetData(lights.data(), 0, lights.size());
            }

            if (volumetricLights.empty()) {
                volumetricLights.emplace_back(Renderer::VolumetricLight {
                    .intensity = 0.0f,
                    .shadowIdx = -1,
                    });
                volumetricShadows.emplace_back(Renderer::Shadow {});
            }                

            if (volumetricLightBuffer.GetElementCount() < volumetricLights.size()) {
                volumetricLightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::VolumetricLight), volumetricLights.size(), volumetricLights.data());
            }
            else {
                volumetricLightBuffer.SetData(volumetricLights.data(), 0, volumetricLights.size());
            }

            if (volumetricShadowBuffer.GetElementCount() < volumetricShadows.size()) {
                volumetricShadowBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                    | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Renderer::Shadow), volumetricShadows.size(), volumetricShadows.data());
            }
            else {
                volumetricShadowBuffer.SetData(volumetricShadows.data(), 0, volumetricShadows.size());
            }

            });

    }

    void SceneRenderState::Extract() {

        // The renderers jitter the copy of the camera, the jitter of the last frame is part of the history
        auto lastCamera = camera;
        hasCamera = scene->HasMainCamera();
        if (hasCamera) {
            camera = scene->GetMainCamera();
            camera.jitterVector = lastCamera.jitterVector;
            camera.lastJitterVector = lastCamera.lastJitterVector;
            camera.jitteredMatrix = lastCamera.jitteredMatrix;
            camera.lastJitteredMatrix = lastCamera.lastJitteredMatrix;
        }

        mainLight.reset();
        allLights.clear();
        auto lightSubset = scene->GetSubset<LightComponent>();
        for (auto& lightEntity : lightSubset) {
            auto& light = lightEntity.GetComponent<LightComponent>();
            auto& extracted = allLights.emplace_back(LightEntity{ lightEntity, light });
            // Scripts can request a shadow update again while this frame renders
            if (light.shadow) {
                extracted.updateShadow = light.shadow->update;
                light.shadow->update = false;
            }
            if (light.type == LightType::DirectionalLight && (!mainLight || !mainLight->isMain))
                mainLight = light;
        }

        texts.clear();
        auto textSubset = scene->GetSubset<TextComponent>();
        for (auto& textEntity : textSubset)
            texts.push_back(textEntity.GetComponent<TextComponent>());

        time = scene->GetTime();
        deltaTime = scene->GetDeltaTime();

        fogVolumes.clear();
        if (hasCamera) {
            auto cameraLocation = camera.GetLocation();
            auto windDirection = Helper::Math::NormalizeOrZero(scene->wind.direction);
            auto windOffset = vec3(windDirection.x, 0.0f, windDirection.y) * scene->wind.speed * time;

            std::vector<std::pair<float, Renderer::FogVolume>> visibleFogVolumes;
            auto fogVolumeSubset = scene->GetSubset<FogVolumeComponent>();
            for (auto& fogVolumeEntity : fogVolumeSubset) {
                const auto& volume = fogVolumeEntity.GetComponent<FogVolumeComponent>();
                auto transform = fogVolumeEntity.TryGetComponent<TransformComponent>();
                if (!volume.enable || volume.density <= 0.0f || !transform)
                    continue;

                auto aabb = Volume::AABB(vec3(-1.0f), vec3(1.0f)).Transform(transform->globalMatrix);
                if (!camera.frustum.Intersects(aabb))
                    continue;

                auto distance = aabb.GetDistance(cameraLocation);
                // Fades out over the last fifth of the distance
                auto fade = volume.fadeDistance > 0.0f ?
                    Helper::Math::Saturate((volume.fadeDistance - distance) / (0.2f * volume.fadeDistance)) : 1.0f;
                if (fade <= 0.0f)
                    continue;

                auto worldToUnit = glm::transpose(glm::inverse(transform->globalMatrix));
                visibleFogVolumes.emplace_back(distance, Renderer::FogVolume {
                    .worldToUnit0 = worldToUnit[0],
                    .worldToUnit1 = worldToUnit[1],
                    .worldToUnit2 = worldToUnit[2],
                    .scatteringAndDensity = vec4(Common::ColorConverter::ConvertSRGBToLinear(volume.scatteringColor), volume.density * fade),
                    .emissiveAndEdgeFade = vec4(Common::ColorConverter::ConvertSRGBToLinear(volume.emissiveColor), volume.edgeFade),
                    .noiseOffsetAndFrequency = vec4(windOffset * volume.windInfluence, 1.0f / volume.noiseScale),
                    .noiseAmount = volume.noiseAmount,
                    .noiseCoverage = volume.noiseCoverage,
                    .noiseSharpness = volume.noiseSharpness,
                    .noiseOctaves = volume.noiseOctaves,
                    .heightFalloff = volume.heightFalloff,
                    .ambientFactor = volume.ambientFactor,
                    .shape = static_cast<int32_t>(volume.shape),
                    .selfShadowing = volume.selfShadowing
                    });
            }

            std::sort(visibleFogVolumes.begin(), visibleFogVolumes.end(),
                [](const auto& volume0, const auto& volume1) { return volume0.first < volume1.first; });
            if (visibleFogVolumes.size() > size_t(Renderer::MAX_FOG_VOLUMES))
                visibleFogVolumes.resize(Renderer::MAX_FOG_VOLUMES);

            for (const auto& [distance, volume] : visibleFogVolumes)
                fogVolumes.push_back(volume);
        }

        auto copy = [](auto& target, const auto& source) {
            if (!source)
                target = nullptr;
            else if (!target)
                target = CreateRef<std::remove_cvref_t<decltype(*source)>>(*source);
            else
                *target = *source;
            };

        wind = scene->wind;

        // Scripts can request new cloud noise or atmosphere tables again while this frame renders. A request stays
        // in the render state until a frame renders it
        auto clouds = sky.clouds;
        auto needsNoiseUpdate = clouds && clouds->needsNoiseUpdate;
        sky = scene->sky;
        copy(clouds, scene->sky.clouds);
        if (clouds) {
            clouds->needsNoiseUpdate = needsNoiseUpdate || scene->sky.clouds->needsNoiseUpdate;
            scene->sky.clouds->needsNoiseUpdate = false;
        }
        sky.clouds = clouds;
        if (scene->sky.atmosphere) {
            atmosphereNeedsUpdate |= scene->sky.atmosphere->needsUpdate;
            scene->sky.atmosphere->needsUpdate = false;
        }
        copy(fog, scene->fog);
        copy(rtgi, scene->rtgi);
        copy(ssgi, scene->ssgi);
        copy(reflection, scene->reflection);
        copy(sss, scene->sss);
        postProcessing = scene->postProcessing;

        irradianceVolume = scene->irradianceVolume && scene->irradianceVolume->enable ? scene->irradianceVolume : nullptr;
        ocean = scene->ocean;
        terrain = scene->terrain;
        clutter = scene->clutter;
        rayTracingWorld = scene->rayTracingWorld;

    }

    const LightEntity* SceneRenderState::GetLight(ECS::Entity entity) const {

        for (const auto& light : allLights) {
            if (light.entity == entity)
                return &light;
        }

        return nullptr;

    }

    void SceneRenderState::WaitForAsyncWorkCompletion() {

        WaitForJobs();

        // A frame that wasn't rendered leaves its render list and its shadow updates behind
        if (!renderList.wasCleared) {
            renderList.Clear();
            for (const auto& light : allLights) {
                if (light.updateShadow)
                    light.comp.shadow->update = true;
            }
        }

    }

    void SceneRenderState::WaitForJobs() {

        JobSystem::Wait(newFrameRenderListJob);
        JobSystem::Wait(bindlessBlasMapUpdateJob);
        JobSystem::Wait(bindlessOtherTextureMapUpdateJob);
        JobSystem::Wait(materialUpdateJob);
        JobSystem::Wait(rayTracingWorldUpdateJob);
        JobSystem::Wait(prepareBindlessBlasesJob);
        JobSystem::Wait(fillMainRenderPassJob);
        JobSystem::Wait(fillShadowRenderPassesJob);
        JobSystem::Wait(fillGPUShadowRenderPassesJob);
        JobSystem::Wait(gpuSceneUpdateJob);
        JobSystem::Wait(cullAndSortLightsJob);

    }

}
