#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void OceanRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            Helper::GeometryHelper::GenerateGridVertexArray(vertexArray, 129, 1.0f / 128.0f, false);

            causticPipelineConfig = PipelineConfig("ocean/caustics.csh");
            underWaterPipelineConfig = PipelineConfig("ocean/underwater.csh");

            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));
            depthUniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));
            lightUniformBuffer = Buffer::UniformBuffer(sizeof(Light));
            cloudShadowUniformBuffer = Buffer::UniformBuffer(sizeof(CloudShadow));

            auto samplerDesc = Graphics::SamplerDesc{
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
            };
            nearestSampler = device->CreateSampler(samplerDesc);

            samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void OceanRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

            if (!scene->ocean || !scene->ocean->enable)
                return;

            Graphics::Profiler::BeginQuery("Ocean");

            auto ocean = scene->ocean;
            auto clouds = scene->sky.clouds;

            auto sun = scene->sky.sun.get();
            if (!sun) {
                auto lights = scene->GetLights();
                for (auto& light : lights) {
                    if (light->type == AE_DIRECTIONAL_LIGHT) {
                        sun = static_cast<Lighting::DirectionalLight*>(light);
                    }
                }

                if (!sun) return;
            }

            vec3 direction = normalize(sun->direction);

            Light lightUniform;
            lightUniform.direction = vec4(sun->direction, 0.0);
            lightUniform.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(sun->color), 0.0);
            lightUniform.intensity = sun->intensity;

            if (sun->GetVolumetric()) {
                target->volumetricTexture.Bind(commandList, 3, 7);
            }

            auto shadow = sun->GetShadow();
            if (shadow) {
                auto distance = !shadow->longRange ? shadow->distance :
                                shadow->longRangeDistance;
                auto& shadowUniform = lightUniform.shadow;
                shadowUniform.distance = distance;
                shadowUniform.bias = shadow->bias;
                shadowUniform.cascadeCount = shadow->componentCount;
                shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                shadowUniform.resolution = vec2(shadow->resolution);

                commandList->BindImage(shadow->maps.image, shadowSampler, 3, 8);

                auto componentCount = shadow->componentCount;
                for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
                    if (i < componentCount) {
                        auto cascade = &shadow->components[i];
                        auto frustum = Volume::Frustum(cascade->frustumMatrix);
                        auto corners = frustum.GetCorners();
                        auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                            abs(corners[1].y - corners[3].y)) / (float)sun->GetShadow()->resolution;
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                        shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                                                                 cascade->viewMatrix * camera->invViewMatrix;
                        shadowUniform.cascades[i].texelSize = texelSize;
                    }
                    else {
                        auto cascade = &shadow->components[componentCount - 1];
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                    }
                }
            }
            else {
                lightUniform.shadow.distance = 0.0f;
            }

            lightUniformBuffer.SetData(&lightUniform, 0);
            lightUniformBuffer.Bind(commandList, 3, 12);

            bool cloudShadowsEnabled = clouds && clouds->enable && clouds->castShadow;

            CloudShadow cloudShadowUniform;

            if (cloudShadowsEnabled) {
                clouds->shadowTexture.Bind(commandList, 3, 15);

                clouds->GetShadowMatrices(camera, glm::normalize(sun->direction),
                    cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera->invViewMatrix;

                cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);
            }

            cloudShadowUniformBuffer.SetData(&cloudShadowUniform, 0);
            cloudShadowUniformBuffer.Bind(commandList, 3, 14);

            {
                Graphics::Profiler::BeginQuery("Caustics");

                const int32_t groupSize = 8;
                auto res = ivec2(target->GetWidth(), target->GetHeight());

                ivec2 groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                causticPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowsEnabled);
                auto pipeline = PipelineManager::GetPipeline(causticPipelineConfig);

                commandList->BindPipeline(pipeline);

                auto lightingImage = target->lightingFrameBuffer->GetColorImage(0);
                auto depthImage = target->lightingFrameBuffer->GetDepthImage();

                commandList->BindImage(depthImage, nearestSampler, 3, 0);
                commandList->BindImage(lightingImage, 3, 1);

                commandList->ImageMemoryBarrier(lightingImage, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                commandList->PushConstants("constants", &ocean->translation.y);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            // Update local texture copies
            {
                auto& colorImage = target->lightingFrameBuffer->GetColorImage(0);
                if (refractionTexture.width != colorImage->width ||
                    refractionTexture.height != colorImage->height ||
                    refractionTexture.format != colorImage->format) {
                    refractionTexture = Texture::Texture2D(colorImage->width, colorImage->height,
                        colorImage->format);
                }

                auto& depthImage = target->lightingFrameBuffer->GetDepthImage();
                if (depthTexture.width != depthImage->width ||
                    depthTexture.height != depthImage->height ||
                    depthTexture.format != depthImage->format) {
                    depthTexture = Texture::Texture2D(depthImage->width, depthImage->height,
                        depthImage->format);
                }

                std::vector<Graphics::ImageBarrier> imageBarriers;
                std::vector<Graphics::BufferBarrier> bufferBarriers;

                imageBarriers = {
                    {colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {depthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {refractionTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    {depthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                commandList->CopyImage(colorImage, refractionTexture.image);
                commandList->CopyImage(depthImage, depthTexture.image);

                imageBarriers = {
                    {colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {depthImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {refractionTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {depthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
            }
            
            {
                Graphics::Profiler::EndAndBeginQuery("Surface");

                commandList->BeginRenderPass(target->lightingFrameBufferWithStencil->renderPass,
                    target->lightingFrameBufferWithStencil);

                auto config = GeneratePipelineConfig(target, false, ocean->wireframe);
                if (cloudShadowsEnabled) config.AddMacro("CLOUD_SHADOWS");
                if (ocean->rippleTexture.IsValid()) config.AddMacro("RIPPLE_TEXTURE");
                if (ocean->foamTexture.IsValid()) config.AddMacro("FOAM_TEXTURE");
                if (scene->terrain && scene->terrain->shoreLine.IsValid()) config.AddMacro("TERRAIN");

                auto pipeline = PipelineManager::GetPipeline(config);

                commandList->BindPipeline(pipeline);

                vertexArray.Bind(commandList);

                Uniforms uniforms = {
                    .waterBodyColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->waterBodyColor), 1.0f),
                    .deepWaterBodyColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->deepWaterBodyColor), 1.0f),
                    .scatterColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->scatterColor), 1.0f),

                    .translation = vec4(ocean->translation, 1.0f),
                    
                    .waterColorIntensity = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->waterColorIntensity), 0.0f, 1.0f),

                    .spectrumTilingFactors = ocean->simulation.spectrumTilingFactors,
                    .spectrumWeights = ocean->simulation.spectrumWeights,
                    .spectrumFadeoutDistances = ocean->simulation.spectrumFadeoutDistances,

                    .displacementScale = ocean->displacementScale,
                    .choppyScale = ocean->choppynessScale,
                    .tiling = ocean->tiling,
                    .hasRippleTexture = ocean->rippleTexture.IsValid() ? 1 : 0,

                    .shoreWaveDistanceOffset = ocean->shoreWaveDistanceOffset,
                    .shoreWaveDistanceScale = ocean->shoreWaveDistanceScale,
                    .shoreWaveAmplitude = ocean->shoreWaveAmplitude,
                    .shoreWaveSteepness = ocean->shoreWaveSteepness,

                    .shoreWavePower = ocean->shoreWavePower,
                    .shoreWaveSpeed = ocean->shoreWaveSpeed,
                    .shoreWaveLength = ocean->shoreWaveLength,
                    .terrainSideLength = -1.0f,
                    .N = ocean->simulation.N,
                    .spectrumCount = ocean->simulation.C,
                };

                ocean->simulation.displacementMap.Bind(commandList, 3, 0);
                ocean->simulation.normalMap.Bind(commandList, 3, 1);

                ocean->foamTexture.Bind(commandList, 3, 2);

                if (scene->sky.GetProbe()) {
                    scene->sky.GetProbe()->cubemap.Bind(commandList, 3, 3);
                }

                refractionTexture.Bind(commandList, 3, 4);
                depthTexture.Bind(commandList, 3, 5);

                if (scene->terrain) {
                    if (scene->terrain->shoreLine.IsValid()) {
                        auto terrain = scene->terrain;

                        uniforms.terrainTranslation = vec4(terrain->translation, 1.0f);
                        uniforms.terrainSideLength = scene->terrain->sideLength;
                        uniforms.terrainHeightScale = scene->terrain->heightScale;

                        scene->terrain->shoreLine.Bind(commandList, 3, 9);

                    }
                }

                if (ocean->rippleTexture.IsValid()) {
                    ocean->rippleTexture.Bind(commandList, 3, 10);
                }

                uniformBuffer.SetData(&uniforms, 0);
                uniformBuffer.Bind(commandList, 3, 11);

                ocean->simulation.perlinNoiseMap.Bind(commandList, 3, 13);
                target->oceanNormalTexture.Bind(commandList, 3, 19);

                auto renderList = ocean->GetRenderList();

                for (auto node : renderList) {

                    PushConstants constants = {
                        .nodeSideLength = node->sideLength,

                        .leftLoD = node->leftLoDStitch,
                        .topLoD = node->topLoDStitch,
                        .rightLoD = node->rightLoDStitch,
                        .bottomLoD = node->bottomLoDStitch,

                        .nodeLocation = node->location,
                    };

                    commandList->PushConstants("constants", &constants);

                    commandList->DrawIndexed(vertexArray.GetIndexComponent().elementCount);

                }

                commandList->EndRenderPass();

                {
                    auto rtData = target->GetHistoryData(FULL_RES);

                    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

                    std::vector<Graphics::BufferBarrier> bufferBarriers;
                    std::vector<Graphics::ImageBarrier> imageBarriers = {
                        {target->lightingTexture.image, layout, access},
                        {rtData->depthTexture->image, layout, access},
                        {rtData->stencilTexture->image, layout, access},
                        {rtData->velocityTexture->image, layout, access},
                    };

                    commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
                }

                if (ocean->underwaterShader) {
                    Graphics::Profiler::EndAndBeginQuery("Underwater");

                    auto& colorImage = target->lightingFrameBuffer->GetColorImage(0);

                    std::vector<Graphics::ImageBarrier> imageBarriers;
                    std::vector<Graphics::BufferBarrier> bufferBarriers;

                    imageBarriers = {
                        {colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                        {refractionTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                    commandList->CopyImage(colorImage, refractionTexture.image);

                    imageBarriers = {
                        {colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {refractionTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    };
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

                    const int32_t groupSize = 8;
                    auto res = ivec2(target->GetWidth(), target->GetHeight());

                    ivec2 groupCount = res / groupSize;
                    groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                    groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                    underWaterPipelineConfig.ManageMacro("TERRAIN", scene->terrain && scene->terrain->shoreLine.IsValid());
                    auto pipeline = PipelineManager::GetPipeline(underWaterPipelineConfig);

                    commandList->BindPipeline(pipeline);

                    auto lightingImage = target->lightingFrameBuffer->GetColorImage(0);
                    auto stencilImage = target->lightingFrameBuffer->GetColorImage(2);
                    auto depthImage = target->lightingFrameBuffer->GetDepthImage();

                    commandList->BindImage(depthImage, nearestSampler, 3, 16);
                    commandList->BindImage(stencilImage, nearestSampler, 3, 17);
                    commandList->BindImage(target->oceanDepthTexture.image, nearestSampler, 3, 18);
                    commandList->BindImage(lightingImage, 3, 20);

                    commandList->ImageMemoryBarrier(lightingImage, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);

                    commandList->ImageMemoryBarrier(lightingImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                }

                Graphics::Profiler::EndQuery();

            }

            Graphics::Profiler::EndQuery();

        }

        void OceanRenderer::RenderDepthOnly(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

            if (!scene->ocean || !scene->ocean->enable)
                return;

            Graphics::Profiler::BeginQuery("Ocean depth");

            auto ocean = scene->ocean;

            Graphics::Profiler::BeginQuery("Rendering");

            commandList->BeginRenderPass(target->oceanDepthOnlyFrameBuffer->renderPass,
                target->oceanDepthOnlyFrameBuffer);

            auto config = GeneratePipelineConfig(target, true, ocean->wireframe);
            if (scene->terrain && scene->terrain->shoreLine.IsValid()) config.AddMacro("TERRAIN");

            auto pipeline = PipelineManager::GetPipeline(config);

            commandList->BindPipeline(pipeline);

            vertexArray.Bind(commandList);

            Uniforms uniforms = {
                .waterBodyColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->waterBodyColor), 1.0f),
                .deepWaterBodyColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->deepWaterBodyColor), 1.0f),
                .scatterColor = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->scatterColor), 1.0f),

                .translation = vec4(ocean->translation, 1.0f),

                .waterColorIntensity = vec4(Common::ColorConverter::ConvertSRGBToLinear(ocean->waterColorIntensity), 0.0f, 1.0f),

                .spectrumTilingFactors = ocean->simulation.spectrumTilingFactors,
                .spectrumWeights = ocean->simulation.spectrumWeights,
                .spectrumFadeoutDistances = ocean->simulation.spectrumFadeoutDistances,

                .displacementScale = ocean->displacementScale,
                .choppyScale = ocean->choppynessScale,
                .tiling = ocean->tiling,
                .hasRippleTexture = ocean->rippleTexture.IsValid() ? 1 : 0,

                .shoreWaveDistanceOffset = ocean->shoreWaveDistanceOffset,
                .shoreWaveDistanceScale = ocean->shoreWaveDistanceScale,
                .shoreWaveAmplitude = ocean->shoreWaveAmplitude,
                .shoreWaveSteepness = ocean->shoreWaveSteepness,

                .shoreWavePower = ocean->shoreWavePower,
                .shoreWaveSpeed = ocean->shoreWaveSpeed,
                .shoreWaveLength = ocean->shoreWaveLength,
                .terrainSideLength = -1.0f,
                .N = ocean->simulation.N,
                .spectrumCount = ocean->simulation.C,
            };

            ocean->simulation.displacementMap.Bind(commandList, 3, 0);
            ocean->simulation.normalMap.Bind(commandList, 3, 1);

            ocean->foamTexture.Bind(commandList, 3, 2);

            if (scene->sky.GetProbe()) {
                scene->sky.GetProbe()->cubemap.Bind(commandList, 3, 3);
            }

            refractionTexture.Bind(commandList, 3, 4);
            depthTexture.Bind(commandList, 3, 5);

            if (scene->terrain) {
                if (scene->terrain->shoreLine.IsValid()) {
                    auto terrain = scene->terrain;

                    uniforms.terrainTranslation = vec4(terrain->translation, 1.0f);
                    uniforms.terrainSideLength = scene->terrain->sideLength;
                    uniforms.terrainHeightScale = scene->terrain->heightScale;

                    scene->terrain->shoreLine.Bind(commandList, 3, 9);

                }
            }

            if (ocean->rippleTexture.IsValid()) {
                ocean->rippleTexture.Bind(commandList, 3, 10);
            }

            depthUniformBuffer.SetData(&uniforms, 0);
            depthUniformBuffer.Bind(commandList, 3, 11);

            ocean->simulation.perlinNoiseMap.Bind(commandList, 3, 13);

            auto renderList = ocean->GetRenderList();

            for (auto node : renderList) {

                PushConstants constants = {
                    .nodeSideLength = node->sideLength,

                    .leftLoD = node->leftLoDStitch,
                    .topLoD = node->topLoDStitch,
                    .rightLoD = node->rightLoDStitch,
                    .bottomLoD = node->bottomLoDStitch,

                    .nodeLocation = node->location,
                };

                commandList->PushConstants("constants", &constants);

                commandList->DrawIndexed(vertexArray.GetIndexComponent().elementCount);

            }

            commandList->EndRenderPass();

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

        PipelineConfig OceanRenderer::GeneratePipelineConfig(RenderTarget* target, bool depthOnly, bool wireframe) {

            const auto shaderConfig = ShaderConfig {
                {depthOnly ? "ocean/depth.vsh" : "ocean/ocean.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {depthOnly ? "ocean/depth.fsh" : "ocean/ocean.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = depthOnly ? target->oceanDepthOnlyFrameBuffer : target->lightingFrameBufferWithStencil,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            pipelineDesc.rasterizer.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }


    }

}