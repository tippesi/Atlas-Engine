#include "PostProcessRenderer.h"

#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void PostProcessRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            sharpenPipelineConfig = PipelineConfig("sharpen.csh");

            Graphics::BufferDesc bufferDesc {
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = sizeof(Uniforms)
            };
            uniformBuffer = device->CreateMultiBuffer(bufferDesc);

        }

        void PostProcessRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene,
            Graphics::CommandList* commandList, Texture::Texture2D* texture) {

            Graphics::Profiler::BeginQuery("Postprocessing");

            auto& postProcessing = scene->postProcessing;

            auto& camera = scene->GetMainCamera();
            const auto& chromaticAberration = postProcessing.chromaticAberration;
            const auto& vignette = postProcessing.vignette;
            const auto& taa = postProcessing.taa;
            auto& sharpen = postProcessing.sharpen;
            auto& bloom = postProcessing.bloom;

            ivec2 resolution = ivec2(target->GetWidth(), target->GetHeight());

            ivec2 groupCount = resolution / 8;
            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            bool fsr2 = postProcessing.fsr2;

            bool spatialUpscalingEnabled = !fsr2 && target->GetScalingFactor() != 1.0f;
            bool shapenEnabled = !fsr2 && sharpen.enable;

            Texture::Texture2D* writeTexture = &target->hdrTexture, *readTexture;

            // Want to have the last pass to always write into the hdr texture
            if (shapenEnabled && spatialUpscalingEnabled) {
                writeTexture = &target->outputTexture;
            }
            else {
                writeTexture = &target->hdrTexture;
            }

            if (fsr2) {
                readTexture = &target->hdrTexture;
            }
            else if (taa.enable){
                readTexture = target->GetHistory();
            }
            else {
                readTexture = &target->lightingTexture;
            }

            if (spatialUpscalingEnabled) {
                Graphics::Profiler::BeginQuery("Scaling");

                auto pipelineConfig = PipelineConfig("upsampleSpatial.csh");
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                commandList->BindPipeline(pipeline);

                const auto& image = writeTexture->image;
                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                commandList->BindImage(image, 3, 0);
                readTexture->Bind(commandList, 3, 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                readTexture = writeTexture;
                writeTexture = &target->hdrTexture;

                Graphics::Profiler::EndQuery();
            }

            if (shapenEnabled) {
                Graphics::Profiler::BeginQuery("Sharpen");

                auto pipelineConfig = PipelineConfig("sharpen.csh");
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                commandList->BindPipeline(pipeline);

                const auto& image = writeTexture->image;
                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                commandList->BindImage(image, 3, 0);
                readTexture->Bind(commandList, 3, 1);

                // Reduce the sharpening to bring it more in line with FSR2 sharpening
                float sharpenFactor = sharpen.factor * 0.5f;
                commandList->PushConstants("constants", &sharpenFactor, sizeof(float));

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                std::swap(readTexture, writeTexture);

                Graphics::Profiler::EndQuery();
            }

            if (bloom.enable) {
                GenerateBloom(bloom, &target->hdrTexture, &target->bloomTexture, commandList);
            }

            {
                Graphics::Profiler::BeginQuery("Main");

                // We can't return here because of the queries
                if (device->swapChain->isComplete) {
                    PipelineConfig pipelineConfig;

                    if (!texture) {
                        commandList->BeginRenderPass(device->swapChain, true);
                        pipelineConfig = GetMainPipelineConfig();
                    }
                    else {
                        commandList->BeginRenderPass(target->outputRenderPass,
                            target->outputFrameBuffer, true);
                        pipelineConfig = GetMainPipelineConfig(target->outputFrameBuffer);
                    }

                    pipelineConfig.ManageMacro("FILMIC_TONEMAPPING", postProcessing.filmicTonemapping);
                    pipelineConfig.ManageMacro("VIGNETTE", postProcessing.vignette.enable);
                    pipelineConfig.ManageMacro("CHROMATIC_ABERRATION", postProcessing.chromaticAberration.enable);
                    pipelineConfig.ManageMacro("FILM_GRAIN", postProcessing.filmGrain.enable);
                    pipelineConfig.ManageMacro("BLOOM", postProcessing.bloom.enable);

                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                    commandList->BindPipeline(pipeline);

                    if (bloom.enable) {
                        target->bloomTexture.Bind(commandList, 3, 1);
                    }

                    SetUniforms(camera, scene);

                    readTexture->Bind(commandList, 3, 0);
                    commandList->BindBuffer(uniformBuffer, 3, 4);

                    commandList->Draw(6, 1, 0, 0);

                    commandList->EndRenderPass();

                    if (texture) {
                        CopyToTexture(&target->outputTexture, texture, commandList);
                    }
                }

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::EndQuery();

        }

        void PostProcessRenderer::Render(Ref<PathTracerRenderTarget> target, Ref<Scene::Scene> scene,
            Graphics::CommandList* commandList, Texture::Texture2D* texture) {

            Graphics::Profiler::BeginQuery("Postprocessing");

            auto& postProcessing = scene->postProcessing;

            auto& camera = scene->GetMainCamera();
            const auto& chromaticAberration = postProcessing.chromaticAberration;
            const auto& vignette = postProcessing.vignette;
            const auto& taa = postProcessing.taa;
            auto& sharpen = postProcessing.sharpen;
            auto& bloom = postProcessing.bloom;

            ivec2 resolution = ivec2(target->GetWidth(), target->GetHeight());

            if (sharpen.enable && !postProcessing.fsr2) {
                Graphics::Profiler::BeginQuery("Sharpen");

                auto pipeline = PipelineManager::GetPipeline(sharpenPipelineConfig);

                commandList->BindPipeline(pipeline);

                ivec2 groupCount = resolution / 8;
                groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

                const auto& image = target->historyPostProcessTexture.image;

                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                commandList->BindImage(image, 3, 0);

                if (taa.enable) {
                    target->postProcessTexture.Bind(commandList, 3, 1);
                }
                else {
                    target->radianceTexture.Bind(commandList, 3, 1);
                }

                // Reduce the sharpening to bring it more in line with FSR2 sharpening
                float sharpenFactor = sharpen.factor * 0.5f;
                commandList->PushConstants("constants", &sharpenFactor, sizeof(float));

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                Graphics::Profiler::EndQuery();
            }

            {
                Graphics::Profiler::BeginQuery("Main");

                // We can't return here because of the queries
                if (device->swapChain->isComplete) {
                    PipelineConfig pipelineConfig;

                    if (!texture) {
                        commandList->BeginRenderPass(device->swapChain, true);
                        pipelineConfig = GetMainPipelineConfig();
                    }
                    else {
                        commandList->BeginRenderPass(target->outputRenderPass,
                            target->outputFrameBuffer, true);
                        pipelineConfig = GetMainPipelineConfig(target->outputFrameBuffer);
                    }

                    pipelineConfig.ManageMacro("FILMIC_TONEMAPPING", postProcessing.filmicTonemapping);
                    pipelineConfig.ManageMacro("VIGNETTE", postProcessing.vignette.enable);
                    pipelineConfig.ManageMacro("CHROMATIC_ABERRATION", postProcessing.chromaticAberration.enable);
                    pipelineConfig.ManageMacro("FILM_GRAIN", postProcessing.filmGrain.enable);

                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                    commandList->BindPipeline(pipeline);

                    SetUniforms(camera, scene);

                    if (sharpen.enable) {
                        target->historyPostProcessTexture.Bind(commandList, 3, 0);
                    }
                    else {
                        if (taa.enable) {
                            target->postProcessTexture.Bind(commandList, 3, 0);
                        }
                        else {
                            target->radianceTexture.Bind(commandList, 3, 0);
                        }
                    }
                    commandList->BindBuffer(uniformBuffer, 3, 4);

                    commandList->Draw(6, 1, 0, 0);

                    commandList->EndRenderPass();

                    if (texture) {
                        CopyToTexture(&target->outputTexture, texture, commandList);
                    }
                }

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::EndQuery();

        }

        void PostProcessRenderer::GenerateBloom(PostProcessing::Bloom& bloom, Texture::Texture2D* hdrTexture, 
            Texture::Texture2D* bloomTexture, Graphics::CommandList* commandList) {
            
            const uint32_t maxDownsampleCount = 12;
            ivec2 resolutions[maxDownsampleCount];

            Graphics::Profiler::BeginQuery("Bloom");
            Graphics::Profiler::BeginQuery("Copy texture");

            CopyToTexture(hdrTexture, bloomTexture, commandList);

            auto mipLevels = std::min(bloom.mipLevels, bloomTexture->image->mipLevels);
            mipLevels = std::min(mipLevels, maxDownsampleCount);

            auto textureIn = bloomTexture;
            auto textureOut = hdrTexture;           

            // Downsample
            {
                Graphics::Profiler::EndAndBeginQuery("Downsample");

                struct PushConstants {
                    int mipLevel;
                    float threshold;
                };

                auto pipelineConfig = PipelineConfig("bloom/bloomDownsample.csh");
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);

                ivec2 resolution = ivec2(bloomTexture->width, bloomTexture->height);
                resolutions[0] = resolution;
                resolution /= 2;
                
                for (int32_t i = 1; i < mipLevels; i++) {
                    std::vector<Graphics::BufferBarrier> bufferBarriers;
                    std::vector<Graphics::ImageBarrier> imageBarriers = {
                        {textureIn->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {textureOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                    };
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                    ivec2 groupCount = resolution / 16;
                    groupCount.x += ((groupCount.x * 16 == resolution.x) ? 0 : 1);
                    groupCount.y += ((groupCount.y * 16 == resolution.y) ? 0 : 1);

                    PushConstants constants {
                        .mipLevel = i - 1,
                        .threshold = bloom.threshold
                    };
                    commandList->PushConstants("constants", &constants);

                    commandList->BindImage(textureOut->image, 3, 0, i);
                    textureIn->Bind(commandList, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                    
                    std::swap(textureIn, textureOut);
                    
                    resolutions[i] = resolution;
                    resolution /= 2;
                }
            }

            // Upsample
            {
                Graphics::Profiler::EndAndBeginQuery("Upsample");

                struct PushConstants {
                    int mipLevel;
                    float filterSize = 2.0f;
                };

                auto pipelineConfig = PipelineConfig("bloom/bloomUpsample.csh");
                pipelineConfig.ManageMacro("DIRT_MAP", bloom.dirtMap.IsLoaded());
                
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);
                
                for (int32_t i = mipLevels - 2; i >= 0; i--) {
                    
                    std::vector<Graphics::BufferBarrier> bufferBarriers;
                    std::vector<Graphics::ImageBarrier> imageBarriers = {
                        {textureIn->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {textureOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                    };
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                    ivec2 groupCount = resolutions[i] / 8;
                    groupCount.x += ((groupCount.x * 8 == resolutions[i].x) ? 0 : 1);
                    groupCount.y += ((groupCount.y * 8 == resolutions[i].y) ? 0 : 1);

                    PushConstants constants {
                        .mipLevel = i + 1,
                        .filterSize = bloom.filterSize,
                    };
                    commandList->PushConstants("constants", &constants);

                    commandList->BindImage(textureOut->image, 3, 0, i);
                    textureIn->Bind(commandList, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                    
                    std::swap(textureIn, textureOut);
                }

                Graphics::Profiler::EndQuery();
                Graphics::Profiler::EndQuery();
            }

        }

        void PostProcessRenderer::CopyToTexture(Texture::Texture2D* sourceTexture, Texture::Texture2D *texture,
            Graphics::CommandList* commandList) {

            auto& srcImage = sourceTexture->image;
            auto& dstImage = texture->image;

            std::vector<Graphics::ImageBarrier> imageBarriers;
            std::vector<Graphics::BufferBarrier> bufferBarriers;

            imageBarriers = {
                {srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->BlitImage(srcImage, dstImage);

            imageBarriers = {
                {srcImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {dstImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        }

        void PostProcessRenderer::SetUniforms(const CameraComponent& camera, Ref<Scene::Scene> scene) {

            const auto& postProcessing = scene->postProcessing;

            const auto& chromaticAberration = postProcessing.chromaticAberration;
            const auto& vignette = postProcessing.vignette;
            const auto& filmGrain = postProcessing.filmGrain;
            const auto& bloom = postProcessing.bloom;

            Uniforms uniforms = {
                .exposure = camera.exposure,
                .paperWhiteLuminance = postProcessing.paperWhiteLuminance,
                .maxScreenLuminance = postProcessing.screenMaxLuminance,
                .saturation = postProcessing.saturation,
                .contrast = postProcessing.contrast,
                .tintColor = vec4(postProcessing.tint, 1.0f)
            };

            if (chromaticAberration.enable) {
                uniforms.aberrationStrength = chromaticAberration.strength;
                uniforms.aberrationReversed = chromaticAberration.colorsReversed ? 1.0f : 0.0f;
            }
            else {
                uniforms.aberrationStrength = 0.0f;
            }

            if (vignette.enable) {
                uniforms.vignetteOffset = vignette.offset;
                uniforms.vignettePower = vignette.power;
                uniforms.vignetteStrength = vignette.strength;
                uniforms.vignetteColor = vec4(vignette.color, 0.0f);
            }

            if (filmGrain.enable) {
                uniforms.filmGrainStrength = filmGrain.strength;
            }

            if (bloom.enable) {
                uniforms.bloomStrength = bloom.strength;
            }

            uniformBuffer->SetData(&uniforms, 0, sizeof(Uniforms));

        }

        PipelineConfig PostProcessRenderer::GetMainPipelineConfig() {

            auto shaderConfig = ShaderConfig {
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .swapChain = device->swapChain
            };
            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

            return PipelineConfig(shaderConfig, pipelineDesc, GetMacros());

        }

        PipelineConfig PostProcessRenderer::GetMainPipelineConfig(const Ref<Graphics::FrameBuffer> frameBuffer) {

            auto shaderConfig = ShaderConfig {
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = frameBuffer
            };
            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            
            return PipelineConfig(shaderConfig, pipelineDesc, GetMacros());

        }

        std::vector<std::string> PostProcessRenderer::GetMacros() const {

            std::vector<std::string> macros;
            if (device->swapChain->IsHDR()) {
                if (device->swapChain->colorSpace == Graphics::HDR10_HLG) {
                    macros.push_back("HYBRID_LOG_GAMMA_EOTF");
                }
                else {
                    // Dolby vision and st2084 use this eotf
                    macros.push_back("PERCEPTUAL_QUANTIZER_EOTF");
                }
                macros.push_back("HDR");
            }
            if (device->swapChain->NeedsGammaCorrection()) {
                macros.push_back("GAMMA_CORRECTION");
            }

            return macros;

        }

    }}
