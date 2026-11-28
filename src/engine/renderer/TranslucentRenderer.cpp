#include "TranslucentRenderer.h"
#include "VolumetricRenderer.h"

#include "../common/ColorConverter.h"
#include "../common/MathHelper.h"
#include "../common/Packing.h"
#include "../materialgraph/PipelineCache.h"

namespace Atlas {

    namespace Renderer {

        void TranslucentRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            lightCullingPipelineConfig = PipelineConfig("deferred/lightCulling.csh", { "FROM_NEAR_PLANE" });
            lightCullingBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));
            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        bool TranslucentRenderer::RenderFrontLayer(Ref<RenderTarget> target, Ref<Scene::Scene> scene,
            Graphics::CommandList* commandList, RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap) {

            auto mainPass = renderList->GetMainPass();
            if (!mainPass)
                return false;

            auto hasAlphaBlended = false;
            ForEachDraw(renderList, *mainPass, DrawPass::FrontLayer, [&](const RenderList::TranslucentInstance&,
                Mesh::MeshSubData&, Mesh::Mesh*, bool) { hasAlphaBlended = true; });
            if (!hasAlphaBlended)
                return false;

            CPUProfiler::Scope profilerScope("Translucent front layer");
            GPUProfiler::Scope gpuScope("Translucent front layer");

            auto depthTexture = target->GetData(FULL_RES)->depthTexture;
            if (!frontLayer.depthTexture || frontLayer.depthTexture->width != depthTexture->width ||
                frontLayer.depthTexture->height != depthTexture->height) {
                frontLayer = RenderTargetData(ivec2(depthTexture->width, depthTexture->height), true, true);
                frontLayerRendering = Graphics::RenderingDesc {
                    .colorAttachments = {
                        {.image = frontLayer.normalTexture->image},
                        {.image = frontLayer.geometryNormalTexture->image},
                        {.image = frontLayer.roughnessMetallicAoTexture->image},
                        {.image = frontLayer.materialIdxTexture->image},
                        {.image = frontLayer.velocityTexture->image},
                    },
                    .depthAttachment = {.image = frontLayer.depthTexture->image}
                };
            }

            // The surfaces are only written where they are in front of the G-buffer
            Graphics::ImageBarrier preCopyBarriers[] = {
                {depthTexture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {frontLayer.depthTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            commandList->PipelineBarrier(preCopyBarriers, {}, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            commandList->CopyImage(depthTexture->image, frontLayer.depthTexture->image);
            Graphics::ImageBarrier postCopyBarriers[] = {
                {depthTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {frontLayer.depthTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(postCopyBarriers, {}, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            commandList->BeginRendering(frontLayerRendering);
            Draw(DrawPass::FrontLayer, target, scene, commandList, renderList, *mainPass, materialMap);
            commandList->EndRendering();

            return true;

        }

        bool TranslucentRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
            RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap, bool reflections) {

            auto mainPass = renderList->GetMainPass();
            if (!mainPass || mainPass->translucentInstances.empty())
                return false;

            CPUProfiler::Scope profilerScope("Translucent surfaces");
            GPUProfiler::Scope gpuScope("Translucent surfaces");

            auto renderState = &scene->renderState;
            auto fog = renderState->fog;
            auto clouds = renderState->sky.clouds;
            auto volume = renderState->irradianceVolume;

            auto fogEnabled = fog && fog->enable;
            auto cloudShadowsEnabled = clouds && clouds->enable && clouds->castShadow && renderState->HasMainLight();
            auto ddgiEnabled = volume && scene->IsRtDataValid();

            sceneMacros.clear();
            if (fogEnabled) sceneMacros.push_back("FOG");
            if (cloudShadowsEnabled) sceneMacros.push_back("CLOUD_SHADOWS");
            if (ddgiEnabled) sceneMacros.push_back("DDGI");
            if (ddgiEnabled && volume->scroll) sceneMacros.push_back("DDGI_SCROLL");
            if (reflections) sceneMacros.push_back("REFLECTION");
            sceneMacrosHash = 0;
            for (const auto& macro : sceneMacros)
                HashCombine(sceneMacrosHash, macro);

            CullLights(target, scene, commandList);

            // Only refracting surfaces read the background and their back faces. New textures are written once
            auto refracts = false;
            ForEachDraw(renderList, *mainPass, DrawPass::Blended, [&](const RenderList::TranslucentInstance&,
                Mesh::MeshSubData& subData, Mesh::Mesh*, bool) { refracts |= subData.material->Refracts(); });
            auto resized = ResizeRefractionTextures(target);
            backgroundCopies.clear();
            if (resized || refracts) {
                CopyBackground(target, commandList, true);
                PlanBackgroundCopies(scene, renderList, *mainPass);

                GPUProfiler::Scope backFacesScope("Refraction back faces");
                commandList->BeginRendering(refractionBackRendering);
                Draw(DrawPass::RefractionBackFaces, target, scene, commandList, renderList, *mainPass, materialMap);
                commandList->EndRendering();
            }

            // The depth and velocity of the target are swapped with their history every frame
            if (translucencyTexture.width != target->lightingTexture.width || translucencyTexture.height != target->lightingTexture.height) {
                translucencyTexture = Texture::Texture2D(target->lightingTexture.width, target->lightingTexture.height,
                    VK_FORMAT_R8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
                translucentRendering = Graphics::RenderingDesc {
                    .colorAttachments = {
                        {},
                        {.image = translucencyTexture.image, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR},
                        {},
                    }
                };
                translucentRendering.colorClearValue = { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } };
            }
            translucentRendering.colorAttachments[0].image = target->lightingTexture.image;
            translucentRendering.colorAttachments[2].image = target->GetData(FULL_RES)->velocityTexture->image;
            translucentRendering.depthAttachment.image = target->GetData(FULL_RES)->depthTexture->image;

            Uniforms uniforms = {
                .resolution = vec2(target->lightingTexture.width, target->lightingTexture.height),
                .froxelGridScale = vec2(1.0f),
                .lightBucketCount = int32_t(std::ceil(float(std::min(4096, int32_t(renderState->lightEntities.size()))) / 32.0f)),
                .lightTileCountX = lightTileCount.x,
                .specularProbeMipLevels = int32_t(renderState->sky.GetProbe() ?
                    renderState->sky.GetProbe()->GetCubemap().image->mipLevels : 1),
                .refractionMipLevels = int32_t(refractionTexture.image->mipLevels),
            };
            if (fogEnabled) {
                auto grid = VolumetricRenderer::GetFroxelGrid(target, scene);
                uniforms.froxelGridScale = vec2(grid.resolution) / vec2(grid.columnCount * grid.tileSize);
                uniforms.froxelNear = grid.near;
                uniforms.froxelFar = grid.far;
                uniforms.froxelExtendedFar = grid.extendedFar;
                uniforms.froxelSliceCount = grid.sliceCount;
                uniforms.froxelDistributionExponent = grid.distributionExponent;
            }
            uniformBuffer.SetData(&uniforms, 0);

            commandList->BeginRendering(translucentRendering);

            commandList->BindSampler(shadowSampler, 3, 11);
            if (cloudShadowsEnabled)
                clouds->shadowTexture.Bind(commandList, 3, 12);
            lightCullingBuffer.Bind(commandList, 3, 13);
            refractionTexture.Bind(commandList, 3, 14);
            refractionDepthTexture.Bind(commandList, 3, 15);
            refractionBackDepthTexture.Bind(commandList, 3, 20);
            refractionBackInstanceTexture.Bind(commandList, 3, 21);
            if (reflections) {
                auto reflectionDepthTexture = target->GetReflectionData(FULL_RES)->depthTexture;
                target->reflectionTexture.Bind(commandList, 3, 16);
                commandList->BindImage(reflectionDepthTexture->image, reflectionDepthTexture->sampler, 3, 17);
            }
            if (fogEnabled)
                target->integratedFroxelTexture.Bind(commandList, 3, 18);
            uniformBuffer.Bind(commandList, 3, 19);

            Draw(DrawPass::Blended, target, scene, commandList, renderList, *mainPass, materialMap);

            commandList->EndRendering();

            return true;

        }

        PipelineConfig TranslucentRenderer::GetPipelineConfig(Mesh::MeshSubData* subData, Mesh::Mesh* mesh,
            const Ref<RenderTarget>& target, DrawPass drawPass, bool backFaces, bool softBody, bool reversedWinding,
            bool& usesGraph) {

            const auto& material = subData->material;
            const auto& graph = material->graph;
            auto compiled = material->HasGraph() && graph.IsLoaded() ? graph->GetCompiled() : nullptr;
            usesGraph = compiled && compiled->valid && !compiled->mainVertexFile.empty();

            auto baseConfig = OpaqueRenderer::GetPipelineConfigForSubData(subData, mesh, target, usesGraph ? compiled.get() : nullptr);

            auto macros = baseConfig.macros;
            std::erase(macros, std::string("DRAW_DATA"));
            if (softBody) {
                std::erase(macros, std::string("INSTANCE_BUFFER"));
                macros.push_back("SOFT_BODY");
            }

            // Graphs give the refractive index per fragment, a pipeline of its own decides whether there is refraction
            if (material->Refracts())
                macros.push_back("REFRACTION");

            auto desc = baseConfig.graphicsPipelineDesc;
            if (drawPass == DrawPass::FrontLayer) {
                macros.push_back("FRONT_LAYER");
                desc.attachmentFormats = Graphics::AttachmentFormats(frontLayerRendering);
            }
            else if (drawPass == DrawPass::RefractionBackFaces) {
                macros.push_back("REFRACTION_BACK_FACES");
                desc.attachmentFormats = Graphics::AttachmentFormats(refractionBackRendering);
                desc.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
            }
            else {
                macros.push_back("TRANSLUCENT");
                macros.insert(macros.end(), sceneMacros.begin(), sceneMacros.end());
                desc.attachmentFormats = Graphics::AttachmentFormats(translucentRendering);
                desc.depthStencilInputInfo.depthWriteEnable = VK_FALSE;

                auto& blend = desc.colorBlendAttachment;
                blend.blendEnable = VK_TRUE;
                blend.colorBlendOp = VK_BLEND_OP_ADD;
                blend.alphaBlendOp = VK_BLEND_OP_ADD;
                if (material->GetBlendMode() == MaterialBlendMode::Multiply) {
                    macros.push_back("BLEND_MULTIPLY");
                    blend.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                    blend.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
                    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                }
                else {
                    // Additive surfaces have no coverage in the alpha of their color
                    macros.push_back(material->GetBlendMode() == MaterialBlendMode::Additive ? "BLEND_ADDITIVE" : "BLEND_ALPHA");
                    blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                }

                if (desc.rasterizer.cullMode == VK_CULL_MODE_NONE)
                    desc.rasterizer.cullMode = backFaces ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
            }

            auto config = PipelineConfig(baseConfig.shaderConfig, desc, macros);
            if (reversedWinding)
                config = config.GetReversedWindingVariant();

            if (usesGraph) {
                auto state = MaterialGraph::PipelineCache::Request(config, graph->blockingPipelineUsers > 0);
                if (state != MaterialGraph::PipelineCache::State::Ready)
                    return PipelineConfig();
            }

            return config;

        }

        template<class DrawSubData>
        void TranslucentRenderer::ForEachDraw(RenderList* renderList, const RenderList::Pass& pass, DrawPass drawPass,
            const DrawSubData& drawSubData) {

            for (const auto& instance : pass.translucentInstances) {
                auto meshIt = renderList->meshIdToMeshMap.find(instance.meshId);
                if (meshIt == renderList->meshIdToMeshMap.end())
                    continue;

                // Two sided surfaces are blended back faces first, closed ones like vessels are in order then
                auto mesh = meshIt->second.Get().get();
                auto sideCount = drawPass == DrawPass::Blended ? 2 : 1;
                for (int32_t side = 0; side < sideCount; side++) {
                    auto backFaces = drawPass == DrawPass::RefractionBackFaces || (sideCount == 2 && side == 0);
                    for (auto& subData : mesh->data.subData) {
                        const auto& material = subData.material;
                        if (!material.IsLoaded() || !material->IsTranslucent())
                            continue;
                        if (drawPass == DrawPass::FrontLayer && material->GetBlendMode() != MaterialBlendMode::Alpha)
                            continue;
                        // Two sided refracting surfaces are thin slabs, their back faces don't end a volume
                        if (drawPass == DrawPass::RefractionBackFaces && (!material->Refracts() || material->IsTwoSided()))
                            continue;
                        if (drawPass == DrawPass::Blended && backFaces && !material->IsTwoSided() && mesh->cullBackFaces)
                            continue;
                        drawSubData(instance, subData, mesh, backFaces);
                    }
                }
            }

        }

        void TranslucentRenderer::Draw(DrawPass drawPass, const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
            Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass& pass,
            std::unordered_map<void*, uint16_t>& materialMap) {

            BindPassBuffers(scene, commandList, renderList, pass);

            const Graphics::Pipeline* currentPipeline = nullptr;
            const Graphics::PushConstantRange* constantsRange = nullptr;
            const Mesh::Mesh* currentMesh = nullptr;
            auto currentGeometryOnly = false;
            ForEachDraw(renderList, pass, drawPass, [&](const RenderList::TranslucentInstance& instance, Mesh::MeshSubData& subData,
                Mesh::Mesh* mesh, bool backFaces) {
                // The streaming might have unloaded the geometry since the instances were culled
                if (!mesh->IsGeometryResident())
                    return;

                // The pipelines are kept with the sub data, the scene macros are part of the variant of the blended pass
                auto reversedWinding = instance.instances.reversedWindingCount > 0;
                Hash variant = 0;
                HashCombine(variant, int32_t(drawPass));
                HashCombine(variant, backFaces);
                HashCombine(variant, instance.softBody);
                HashCombine(variant, reversedWinding);
                if (drawPass == DrawPass::Blended)
                    HashCombine(variant, sceneMacrosHash);
                auto cached = std::find_if(subData.translucentPipelines.begin(), subData.translucentPipelines.end(),
                    [&](const Mesh::MeshSubDataTranslucentPipeline& pipeline) { return pipeline.variant == variant; });
                if (cached == subData.translucentPipelines.end()) {
                    bool usesGraph = false;
                    auto config = GetPipelineConfig(&subData, mesh, target, drawPass, backFaces, instance.softBody, reversedWinding, usesGraph);
                    // Graphs are compiled in the background, their pipeline is looked up again on a later draw
                    if (!config.IsValid())
                        return;
                    subData.translucentPipelines.push_back({ variant, usesGraph, config });
                    cached = subData.translucentPipelines.end() - 1;
                }
                auto usesGraph = cached->usesGraph;

                auto pipeline = PipelineManager::GetPipeline(cached->config);
                if (!pipeline->isComplete)
                    return;

                auto instanceIdx = size_t(&instance - pass.translucentInstances.data());
                if (drawPass == DrawPass::Blended && !backgroundCopies.empty() && instanceIdx >= backgroundCopies.back()) {
                    backgroundCopies.pop_back();
                    commandList->EndRendering();
                    CopyBackground(target, commandList, false);
                    auto continuedRendering = translucentRendering;
                    continuedRendering.colorAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    commandList->BeginRendering(continuedRendering);
                    currentPipeline = nullptr;
                    currentMesh = nullptr;
                }

                if (pipeline.get() != currentPipeline) {
                    commandList->BindPipeline(pipeline);
                    currentPipeline = pipeline.get();
                    constantsRange = pipeline->shader->GetPushConstantRange("constants");
                }

                // Soft bodies use all vertices of their mesh, the other instances its geometry range
                const auto& range = mesh->GetGeometryRange();
                auto geometryOnly = range && !instance.softBody;
                if (mesh != currentMesh || geometryOnly != currentGeometryOnly) {
                    mesh->vertexArray.Bind(commandList, !geometryOnly);
                    currentMesh = mesh;
                    currentGeometryOnly = geometryOnly;
                }

                auto opaqueConstants = OpaqueRenderer::GetPushConstants({ &subData, instance.meshId, mesh, nullptr, instance.softBody },
                    usesGraph, scene.get(), materialMap);
                const auto& material = *subData.material.Get();
                PushConstants pushConstants = {
                    .draw = opaqueConstants.draw,
                    .deformOffset = uint32_t(instance.instances.deformOffset),
                    .jointCount = opaqueConstants.jointCount,
                    .instanceOffset = uint32_t(instance.instances.offset),
                    .firstDraw = 0,
                    .refractiveIndex = material.Refracts() ? material.refractiveIndex : 1.0f,
                    .thickness = material.thickness,
                    .attenuationColor = uint32_t(Common::Packing::PackUnsignedVector3x10_1x2(
                        vec4(::Helper::Math::Saturate(Common::ColorConverter::ConvertSRGBToLinear(material.attenuationColor)), 0.0f))),
                    .attenuationDistance = material.attenuationDistance,
                    .materialGraphOffset = opaqueConstants.materialGraphOffset
                };

                auto firstIndex = geometryOnly ? range->firstIndex : 0u;
                auto vertexOffset = geometryOnly ? int32_t(range->firstVertex) : 0;
                RenderList::DrawInstances(commandList, subData, instance.instances, reversedWinding, firstIndex, vertexOffset,
                    [&](uint32_t lod) {
                        pushConstants.draw.lod = lod;
                        commandList->PushConstants(constantsRange, &pushConstants);
                    });
                });

        }

        void TranslucentRenderer::BindPassBuffers(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
            RenderList* renderList, const RenderList::Pass& pass) {

            commandList->BindBuffer(pass.currentMatricesBuffer, 1, 1);
            commandList->BindBuffer(pass.lastMatricesBuffer, 1, 2);
            commandList->BindBuffer(pass.currentJointMatricesBuffer, 1, 4);
            commandList->BindBuffer(pass.lastJointMatricesBuffer, 1, 5);
            // The instances are culled on the host, the GPU scene binds the indices of its culling here
            commandList->BindBuffer(pass.instanceIndicesBuffer, 1, 22);
            if (pass.softBodyVertexOffsetsBuffer) {
                commandList->BindBuffer(renderList->currentSoftBodyVerticesBuffer, 1, 24);
                commandList->BindBuffer(renderList->lastSoftBodyVerticesBuffer, 1, 25);
                commandList->BindBuffer(pass.softBodyVertexOffsetsBuffer, 1, 26);
            }

            scene->renderState.wind.noiseMap.Bind(commandList, 3, 8);
            if (!scene->renderState.materialGraphParameters.empty())
                scene->renderState.materialGraphBuffer.Bind(commandList, 3, 9);

        }

        void TranslucentRenderer::CullLights(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            GPUProfiler::Scope gpuScope("Light culling");

            const int32_t groupSize = 16;
            ivec2 res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            lightTileCount = res / groupSize;
            lightTileCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            lightTileCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            if (lightCullingBuffer.GetElementCount() < size_t(lightTileCount.x * lightTileCount.y * 128))
                lightCullingBuffer.SetSize(lightTileCount.x * lightTileCount.y * 128);

            commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            auto pipeline = PipelineManager::GetPipeline(lightCullingPipelineConfig);
            commandList->BindPipeline(pipeline);

            int32_t lightCount = std::min(4096, int32_t(scene->renderState.lightEntities.size()));
            commandList->PushConstants("constants", &lightCount);
            lightCullingBuffer.Bind(commandList, 3, 6);

            commandList->Dispatch(lightTileCount.x, lightTileCount.y, 1);

            commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        }

        bool TranslucentRenderer::ResizeRefractionTextures(const Ref<RenderTarget>& target) {

            auto lightingImage = target->lightingTexture.image;
            auto depthImage = target->GetData(FULL_RES)->depthTexture->image;
            if (refractionTexture.width == int32_t(lightingImage->width) && refractionTexture.height == int32_t(lightingImage->height))
                return false;

            refractionTexture = Texture::Texture2D(lightingImage->width, lightingImage->height, lightingImage->format,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
            refractionDepthTexture = Texture::Texture2D(depthImage->width, depthImage->height, depthImage->format,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
            refractionBackDepthTexture = Texture::Texture2D(depthImage->width, depthImage->height, VK_FORMAT_D32_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            refractionBackInstanceTexture = Texture::Texture2D(depthImage->width, depthImage->height, VK_FORMAT_R16_UINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            refractionBackRendering = Graphics::RenderingDesc {
                .colorAttachments = {
                    {.image = refractionBackInstanceTexture.image, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR}
                },
                .depthAttachment = {.image = refractionBackDepthTexture.image, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR}
            };
            return true;

        }

        void TranslucentRenderer::PlanBackgroundCopies(const Ref<Scene::Scene>& scene, RenderList* renderList,
            const RenderList::Pass& pass) {

            refractingRectangles.clear();

            const auto& camera = scene->renderState.camera;
            auto viewProjectionMatrix = camera.projectionMatrix * camera.viewMatrix;
            // Minimum and maximum on the screen, boxes around the near plane cover all of it
            auto getRectangle = [&](const Volume::AABB& aabb) {
                auto rectangle = vec4(vec2(std::numeric_limits<float>::max()), vec2(std::numeric_limits<float>::lowest()));
                for (int32_t i = 0; i < 8; i++) {
                    auto corner = glm::mix(aabb.min, aabb.max, glm::bvec3(i & 1, i & 2, i & 4));
                    auto projected = viewProjectionMatrix * vec4(corner, 1.0f);
                    if (projected.w < camera.nearPlane)
                        return vec4(-1.0f, -1.0f, 1.0f, 1.0f);
                    auto position = vec2(projected) / projected.w;
                    rectangle = vec4(glm::min(vec2(rectangle), position), glm::max(vec2(rectangle.z, rectangle.w), position));
                }
                return rectangle;
            };
            auto overlaps = [](vec4 rectangle0, vec4 rectangle1) {
                return rectangle0.x < rectangle1.z && rectangle1.x < rectangle0.z &&
                    rectangle0.y < rectangle1.w && rectangle1.y < rectangle0.w;
            };

            // From near to far: an instance behind refracting ones needs a copy after it, which then serves all of them
            auto displacement = 2.0f * maxRefractionDisplacement * vec2(1.0f / camera.aspectRatio, 1.0f);
            for (auto i = pass.translucentInstances.size(); i-- > 0 && backgroundCopies.size() < maxBackgroundCopies;) {
                const auto& instance = pass.translucentInstances[i];
                auto meshIt = renderList->meshIdToMeshMap.find(instance.meshId);
                if (meshIt == renderList->meshIdToMeshMap.end() || !meshIt->second->IsGeometryResident())
                    continue;

                auto rectangle = getRectangle(instance.aabb);
                if (std::any_of(refractingRectangles.begin(), refractingRectangles.end(),
                    [&](vec4 refracting) { return overlaps(rectangle, refracting); })) {
                    backgroundCopies.push_back(i + 1);
                    refractingRectangles.clear();
                }

                const auto& subData = meshIt->second->data.subData;
                if (std::any_of(subData.begin(), subData.end(), [](const Mesh::MeshSubData& subData) {
                    return subData.material.IsLoaded() && subData.material->Refracts(); })) {
                    refractingRectangles.push_back(rectangle + vec4(-displacement, displacement));
                }
            }

        }

        void TranslucentRenderer::CopyBackground(const Ref<RenderTarget>& target, Graphics::CommandList* commandList, bool copyDepth) {

            GPUProfiler::Scope gpuScope("Background copy");

            auto lightingImage = target->lightingTexture.image;
            auto depthImage = target->GetData(FULL_RES)->depthTexture->image;

            std::vector<Graphics::ImageBarrier> preCopyBarriers = {
                {lightingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {refractionTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            if (copyDepth) {
                preCopyBarriers.push_back({depthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT});
                preCopyBarriers.push_back({refractionDepthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT});
            }
            commandList->PipelineBarrier(preCopyBarriers, {}, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->CopyImage(lightingImage, refractionTexture.image);
            if (copyDepth)
                commandList->CopyImage(depthImage, refractionDepthTexture.image);
            commandList->GenerateMipMaps(refractionTexture.image);

            std::vector<Graphics::ImageBarrier> postCopyBarriers = {
                {lightingImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            if (copyDepth) {
                postCopyBarriers.push_back({depthImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
                postCopyBarriers.push_back({refractionDepthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            }
            commandList->PipelineBarrier(postCopyBarriers, {}, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        }

    }

}
