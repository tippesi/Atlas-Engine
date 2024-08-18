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

            std::vector<uint8_t> dummyData = { 0 };
            dummyTexture = Texture::Texture2D(1, 1, VK_FORMAT_R8_UNORM);
            dummyTexture.SetData(dummyData);

        }

        void OceanRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            if (!scene->ocean || !scene->ocean->enable)
                return;

            Graphics::Profiler::BeginQuery("Ocean");

            auto& camera = scene->GetMainCamera();
            auto ocean = scene->ocean;
            auto clouds = scene->sky.clouds;
            auto fog = scene->fog;

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!mainLightEntity.IsValid())
                return;

            auto& light = mainLightEntity.GetComponent<LightComponent>();

            vec3 direction = normalize(light.transformedProperties.directional.direction);

            Light lightUniform;
            lightUniform.direction = vec4(light.transformedProperties.directional.direction, 0.0);
            lightUniform.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), 0.0);
            lightUniform.intensity = light.intensity;

            if (fog && fog->enable && fog->rayMarching) {
                target->volumetricTexture.Bind(commandList, 3, 7);
            }

            auto shadow = light.shadow;
            if (shadow) {
                auto distance = !shadow->longRange ? shadow->distance :
                                shadow->longRangeDistance;
                auto& shadowUniform = lightUniform.shadow;
                shadowUniform.distance = distance;
                shadowUniform.bias = shadow->bias;
                shadowUniform.edgeSoftness = shadow->edgeSoftness;
                shadowUniform.cascadeCount = shadow->viewCount;
                shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                shadowUniform.resolution = vec2(shadow->resolution);

                commandList->BindImage(shadow->maps->image, shadowSampler, 3, 8);

                auto componentCount = shadow->viewCount;
                for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                    if (i < componentCount) {
                        auto cascade = &shadow->views[i];
                        auto frustum = Volume::Frustum(cascade->frustumMatrix);
                        auto corners = frustum.GetCorners();
                        auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                            abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                        shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                                                                 cascade->viewMatrix * camera.invViewMatrix;
                        shadowUniform.cascades[i].texelSize = texelSize;
                    }
                    else {
                        auto cascade = &shadow->views[componentCount - 1];
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                    }
                }
            }
            else {
                lightUniform.shadow.distance = 0.0f;
            }

            lightUniformBuffer.SetData(&lightUniform, 0);
            lightUniformBuffer.Bind(commandList, 3, 12);

            bool fogEnabled = fog && fog->enable;
            bool cloudsEnabled = clouds && clouds->enable;

            bool cloudShadowsEnabled = clouds && clouds->enable && clouds->castShadow;

            CloudShadow cloudShadowUniform;

            if (cloudShadowsEnabled) {
                clouds->shadowTexture.Bind(commandList, 3, 15);

                clouds->GetShadowMatrices(camera, glm::normalize(light.transformedProperties.directional.direction),
                    cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera.invViewMatrix;

                cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);
            }

            cloudShadowUniformBuffer.SetData(&cloudShadowUniform, 0);
            cloudShadowUniformBuffer.Bind(commandList, 3, 14);

            {
                Graphics::Profiler::BeginQuery("Caustics");

                const int32_t groupSize = 8;
                auto res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

                ivec2 groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                causticPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowsEnabled);
                auto pipeline = PipelineManager::GetPipeline(causticPipelineConfig);

                commandList->BindPipeline(pipeline);

                auto lightingImage = target->afterLightingFrameBuffer->GetColorImage(0);
                auto depthImage = target->afterLightingFrameBuffer->GetDepthImage();

                commandList->BindImage(depthImage, nearestSampler, 3, 0);
                commandList->BindImage(lightingImage, 3, 1);

                commandList->ImageMemoryBarrier(lightingImage, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                commandList->PushConstants("constants", &ocean->translation.y, sizeof(float));

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            // Update local texture copies
            {
                auto& colorImage = target->afterLightingFrameBuffer->GetColorImage(0);
                if (refractionTexture.width != colorImage->width ||
                    refractionTexture.height != colorImage->height ||
                    refractionTexture.format != colorImage->format) {
                    refractionTexture = Texture::Texture2D(colorImage->width, colorImage->height,
                        colorImage->format);
                }

                auto& depthImage = target->afterLightingFrameBuffer->GetDepthImage();
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

                commandList->BeginRenderPass(target->afterLightingFrameBufferWithStencil->renderPass,
                    target->afterLightingFrameBufferWithStencil);

                auto config = GeneratePipelineConfig(target, false, ocean->wireframe);
                if (fogEnabled) config.AddMacro("FOG");
                if (cloudsEnabled) config.AddMacro("CLOUDS");
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

                if (ocean->foamTexture.IsValid()) {
                    ocean->foamTexture.Bind(commandList, 3, 2);
                }
                else {
                    dummyTexture.Bind(commandList, 3, 2);
                }

                if (scene->sky.GetProbe()) {
                    scene->sky.GetProbe()->GetCubemap().Bind(commandList, 3, 3);
                }

                if (cloudsEnabled) {
                    target->volumetricCloudsTexture.Bind(commandList, 3, 16);
                    uniforms.innerCloudRadius = scene->sky.planetRadius + clouds->minHeight;
                }

                refractionTexture.Bind(commandList, 3, 4);
                depthTexture.Bind(commandList, 3, 5);
                target->oceanDepthTexture.Bind(commandList, 3, 20);

                if (scene->terrain) {
                    if (scene->terrain->shoreLine.IsValid()) {
                        auto terrain = scene->terrain;

                        uniforms.terrainTranslation = vec4(terrain->translation, 1.0f);
                        uniforms.terrainSideLength = scene->terrain->sideLength;
                        uniforms.terrainHeightScale = scene->terrain->heightScale;

                        scene->terrain->shoreLine.Bind(commandList, 3, 9);

                    }
                }

                if (fogEnabled) {
                    target->volumetricTexture.Bind(commandList, 3, 7);

                    auto& fogUniform = uniforms.fog;
                    fogUniform.extinctionCoefficient = fog->extinctionCoefficients;
                    fogUniform.scatteringFactor = fog->scatteringFactor;
                    fogUniform.extinctionFactor = fog->extinctionFactor;
                    fogUniform.density = fog->density;
                    fogUniform.heightFalloff = fog->heightFalloff;
                    fogUniform.height = fog->height;
                    fogUniform.ambientFactor = fog->ambientFactor;
                    fogUniform.scatteringAnisotropy = glm::clamp(fog->scatteringAnisotropy, -0.999f, 0.999f);
                }

                if (ocean->rippleTexture.IsValid()) {
                    ocean->rippleTexture.Bind(commandList, 3, 10);
                }
                else {
                    dummyTexture.Bind(commandList, 3, 10);
                }

                uniformBuffer.SetData(&uniforms, 0);
                uniformBuffer.Bind(commandList, 3, 11);

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

                    auto& colorImage = target->afterLightingFrameBuffer->GetColorImage(0);

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
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                    const int32_t groupSize = 8;
                    auto res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

                    ivec2 groupCount = res / groupSize;
                    groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                    groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                    underWaterPipelineConfig.ManageMacro("TERRAIN", scene->terrain && scene->terrain->shoreLine.IsValid());
                    auto pipeline = PipelineManager::GetPipeline(underWaterPipelineConfig);

                    commandList->BindPipeline(pipeline);

                    auto lightingImage = target->afterLightingFrameBuffer->GetColorImage(0);
                    auto stencilImage = target->afterLightingFrameBuffer->GetColorImage(2);
                    auto depthImage = target->afterLightingFrameBuffer->GetDepthImage();

                    refractionTexture.Bind(commandList, 3, 4);
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

        void OceanRenderer::RenderDepthOnly(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

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
                scene->sky.GetProbe()->GetCubemap().Bind(commandList, 3, 3);
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

        PipelineConfig OceanRenderer::GeneratePipelineConfig(Ref<RenderTarget> target, bool depthOnly, bool wireframe) {

            const auto shaderConfig = ShaderConfig {
                {depthOnly ? "ocean/depth.vsh" : "ocean/ocean.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {depthOnly ? "ocean/depth.fsh" : "ocean/ocean.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = depthOnly ? target->oceanDepthOnlyFrameBuffer : target->afterLightingFrameBufferWithStencil,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            pipelineDesc.rasterizer.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }


    }

}