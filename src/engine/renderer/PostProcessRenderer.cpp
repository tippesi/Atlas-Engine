#include "PostProcessRenderer.h"


namespace Atlas {

    namespace Renderer {

        void PostProcessRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            sharpenPipelineConfig = PipelineConfig("sharpen.csh");

            Graphics::BufferDesc bufferDesc{
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = sizeof(Uniforms)
            };
            uniformBuffer = device->CreateMultiBuffer(bufferDesc);

        }

        void PostProcessRenderer::Render(Ref<RenderTarget>& target, Ref<Scene::Scene>& scene,
            Graphics::CommandList* commandList, bool upscalerSharpens, Texture::Texture2D* texture) {

            CPUProfiler::Scope profilerScope("Postprocessing");
            GPUProfiler::BeginQuery("Postprocessing");

            auto& postProcessing = scene->renderState.postProcessing;

            const auto& chromaticAberration = postProcessing.chromaticAberration;
            const auto& vignette = postProcessing.vignette;
            auto& sharpen = postProcessing.sharpen;
            const auto& bloom = postProcessing.bloom;

            ivec2 resolution = ivec2(target->GetWidth(), target->GetHeight());

            ivec2 groupCount = resolution / 8;
            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            // Temporal upscalers write the full resolution into the hdr texture
            auto upscaler = postProcessing.upscaler;
            bool temporalUpscaling = upscaler != PostProcessing::Upscaler::None && upscaler != PostProcessing::Upscaler::TAA;
            bool autoExposure = postProcessing.autoExposure.enable;
            bool spatialUpscalingEnabled = !temporalUpscaling && target->GetScalingFactor() != 1.0f;
            bool shapenEnabled = !upscalerSharpens && sharpen.enable;

            Texture::Texture2D* writeTexture = &target->hdrTexture, * readTexture;

            if (autoExposure) {
                GenerateExposureTexture(target, scene, commandList);
            }

            // Want to have the last pass to always write into the hdr texture
            if (shapenEnabled && (spatialUpscalingEnabled || temporalUpscaling)) {
                writeTexture = &target->outputTexture;
            }
            else {
                writeTexture = &target->hdrTexture;
            }

            if (temporalUpscaling) {
                readTexture = &target->hdrTexture;
            }
            else if (upscaler == PostProcessing::Upscaler::TAA) {
                readTexture = target->GetHistory();
            }
            else {
                readTexture = &target->lightingTexture;
            }

            if (spatialUpscalingEnabled) {
                GPUProfiler::Scope gpuScope("Scaling");

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
            }

            if (shapenEnabled) {
                GPUProfiler::Scope gpuScope("Sharpen");

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

                // The sharpened upscaler image was written to the output texture, which the main pass can render to
                if (temporalUpscaling) {
                    CopyToTexture(readTexture, &target->hdrTexture, commandList);
                    readTexture = &target->hdrTexture;
                }
            }

            if (bloom.enable) {
                GenerateBloom(bloom, readTexture, &target->hdrTexture, &target->bloomTexture, commandList);
            }

            {
                GPUProfiler::BeginQuery("Main");

                // We can't return here because of the queries
                if (device->swapChain->isComplete) {
                    PipelineConfig pipelineConfig;

                    if (!texture) {
                        commandList->BeginRenderPass(device->swapChain, true);
                        pipelineConfig = GetMainPipelineConfig();
                    }
                    else {
                        commandList->BeginRendering(target->outputRendering);
                        pipelineConfig = GetMainPipelineConfig(Graphics::AttachmentFormats(target->outputRendering));
                    }

                    pipelineConfig.ManageMacro("FILMIC_TONEMAPPING", postProcessing.filmicTonemapping);
                    pipelineConfig.ManageMacro("VIGNETTE", postProcessing.vignette.enable);
                    pipelineConfig.ManageMacro("CHROMATIC_ABERRATION", postProcessing.chromaticAberration.enable);
                    pipelineConfig.ManageMacro("FILM_GRAIN", postProcessing.filmGrain.enable);
                    pipelineConfig.ManageMacro("BLOOM", postProcessing.bloom.enable);
                    pipelineConfig.ManageMacro("BLOOM_DIRT", postProcessing.bloom.enable && postProcessing.bloom.dirtMap.IsLoaded());
                    pipelineConfig.ManageMacro("AUTO_EXPOSURE", autoExposure);

                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                    commandList->BindPipeline(pipeline);

                    if (bloom.enable) {
                        target->bloomTexture.Bind(commandList, 3, 1);
                        if (bloom.dirtMap.IsLoaded())
                            bloom.dirtMap->Bind(commandList, 3, 2);
                    }

                    if (autoExposure) {
                        target->exposureTexture.Bind(commandList, 3, 5);
                    }

                    SetUniforms(target, scene);

                    readTexture->Bind(commandList, 3, 0);
                    commandList->BindBuffer(uniformBuffer, 3, 4);

                    commandList->Draw(6, 1, 0, 0);

                    if (!texture) {
                        commandList->EndRenderPass();
                    }
                    else {
                        commandList->EndRendering();
                        CopyToTexture(&target->outputTexture, texture, commandList);
                    }
                }

                GPUProfiler::EndQuery();
            }

            GPUProfiler::EndQuery();

        }

        void PostProcessRenderer::GenerateExposureTexture(Ref<RenderTarget>& target,
            Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) {

            auto& exposure = scene->renderState.postProcessing.autoExposure;

            GPUProfiler::Scope gpuScope("Auto-exposure");

            float logLuminanceMin = log2(exposure.luminanceMin);
            float logLuminanceRange = log2(exposure.luminanceMax - exposure.luminanceMin);

            // The histogram only samples every second pixel in each direction
            ivec2 histogramResolution = (ivec2(target->GetScaledWidth(), target->GetScaledHeight()) + 1) / 2;

            {
                GPUProfiler::Scope sectionScope("Generate exposure histogram");
                struct alignas(16) HistogramPushConstants {
                    float logLuminanceMin;
                    float invLogLuminanceRange;
                    float blackLevel;
                };

                // The bins are of the luminance without the pre-exposure, the lighting carries it
                auto pushConstants = HistogramPushConstants{
                    .logLuminanceMin = logLuminanceMin + log2(scene->renderState.preExposure),
                    .invLogLuminanceRange = 1.0f / logLuminanceRange,
                    .blackLevel = exposure.blackLevel * scene->renderState.preExposure
                };

                ivec2 groupCount = histogramResolution / 16;
                groupCount.x += ((groupCount.x * 16 == histogramResolution.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == histogramResolution.y) ? 0 : 1);

                auto pipelineConfig = PipelineConfig("exposure/histogram.csh");
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &pushConstants);

                target->lightingTexture.Bind(commandList, 3, 0);
                exposure.histogramBuffer.Bind(commandList, 3, 1);

                Graphics::ImageBarrier histogramImageBarriers[] = {
                    {target->lightingTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                Graphics::BufferBarrier histogramBufferBarriers[] = {
                    {exposure.histogramBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(histogramImageBarriers, histogramBufferBarriers);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            {
                GPUProfiler::Scope sectionScope("Generate exposure texture");

                struct alignas(16) TemporalPushConstants {
                    float logLuminanceMin;
                    float logLuminanceRange;
                    float timeCoefficient;
                    float pixelCount;
                    int32_t resetHistory;
                };

                auto pushConstants = TemporalPushConstants{
                    .logLuminanceMin = logLuminanceMin,
                    .logLuminanceRange = logLuminanceRange,
                    .timeCoefficient = 1.0f - exp(-scene->renderState.deltaTime * exposure.timeCoefficient),
                    .pixelCount = float(histogramResolution.x * histogramResolution.y),
                    .resetHistory = !target->HasHistory() ? 1 : 0
                };

                auto pipelineConfig = PipelineConfig("exposure/temporal.csh");
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &pushConstants);

                commandList->BindImage(target->exposureTexture.image, 3, 0);

                Graphics::ImageBarrier temporalImageBarriers[] = {
                    {target->exposureTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT},
                };
                Graphics::BufferBarrier temporalBufferBarriers[] = {
                    {exposure.histogramBuffer.Get(), VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(temporalImageBarriers, temporalBufferBarriers);

                commandList->Dispatch(1, 1, 1);

                commandList->ImageMemoryBarrier(target->exposureTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

        }

        void PostProcessRenderer::GenerateBloom(const PostProcessing::Bloom& bloom, Texture::Texture2D* sourceTexture,
            Texture::Texture2D* hdrTexture, Texture::Texture2D* bloomTexture, Graphics::CommandList* commandList) {

            const uint32_t maxDownsampleCount = 12;
            ivec2 resolutions[maxDownsampleCount];

            GPUProfiler::BeginQuery("Bloom");
            GPUProfiler::BeginQuery("Copy texture");

            // The lower mips of the hdr texture are only used as scratch space for the downsample and upsample
            CopyToTexture(sourceTexture, bloomTexture, commandList);

            auto mipLevels = std::min(bloom.mipLevels, bloomTexture->image->mipLevels);
            mipLevels = std::min(mipLevels, maxDownsampleCount);

            auto textureIn = bloomTexture;
            auto textureOut = hdrTexture;

            // Downsample
            {
                GPUProfiler::EndAndBeginQuery("Downsample");

                struct alignas(16) PushConstants {
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
                    Graphics::ImageBarrier imageBarriers[] = {
                        {textureIn->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {textureOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                    };
                    commandList->PipelineBarrier(imageBarriers, {});

                    ivec2 groupCount = resolution / 16;
                    groupCount.x += ((groupCount.x * 16 == resolution.x) ? 0 : 1);
                    groupCount.y += ((groupCount.y * 16 == resolution.y) ? 0 : 1);

                    PushConstants constants{
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
                GPUProfiler::EndAndBeginQuery("Upsample");

                struct alignas(16) PushConstants {
                    int additive;
                    int mipLevel;
                    float filterSize = 2.0f;
                };

                auto pipelineConfig = PipelineConfig("bloom/bloomUpsample.csh");
                pipelineConfig.ManageMacro("DIRT_MAP", bloom.dirtMap.IsLoaded());

                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);

                for (int32_t i = mipLevels - 2; i >= 0; i--) {
                    Graphics::ImageBarrier imageBarriers[] = {
                        {textureIn->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {textureOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                    };
                    commandList->PipelineBarrier(imageBarriers, {});

                    ivec2 groupCount = resolutions[i] / 8;
                    groupCount.x += ((groupCount.x * 8 == resolutions[i].x) ? 0 : 1);
                    groupCount.y += ((groupCount.y * 8 == resolutions[i].y) ? 0 : 1);

                    PushConstants constants{
                        .additive = i != mipLevels - 2 ? 1 : 0,
                        .mipLevel = i + 1,
                        .filterSize = bloom.filterSize,
                    };
                    commandList->PushConstants("constants", &constants);

                    commandList->BindImage(textureOut->image, 3, 0, i);
                    textureIn->Bind(commandList, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);

                    std::swap(textureIn, textureOut);
                }

                GPUProfiler::EndQuery();
                GPUProfiler::EndQuery();
            }

            Graphics::ImageBarrier imageBarriers[] = {
                {textureIn->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {textureOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT}
            };
            commandList->PipelineBarrier(imageBarriers, {}, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        }

        void PostProcessRenderer::CopyToTexture(Texture::Texture2D* sourceTexture, Texture::Texture2D* texture,
            Graphics::CommandList* commandList) {

            auto& srcImage = sourceTexture->image;
            auto& dstImage = texture->image;

            Graphics::ImageBarrier preImageBarriers[] = {
                {srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            commandList->PipelineBarrier(preImageBarriers, {},
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->BlitImage(srcImage, dstImage);

            Graphics::ImageBarrier postImageBarriers[] = {
                {srcImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {dstImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(postImageBarriers, {},
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        }

        void PostProcessRenderer::SetUniforms(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene) {

            const auto& postProcessing = scene->renderState.postProcessing;

            const auto& chromaticAberration = postProcessing.chromaticAberration;
            const auto& vignette = postProcessing.vignette;
            const auto& filmGrain = postProcessing.filmGrain;
            const auto& bloom = postProcessing.bloom;

            Uniforms uniforms = {
                .autoExposureMipLevel = float(target->lightingTexture.image->mipLevels - 1),
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
                uniforms.bloomDirtStrength = bloom.dirtStrength;
            }

            uniformBuffer->SetData(&uniforms, 0, sizeof(Uniforms));

        }

        PipelineConfig PostProcessRenderer::GetMainPipelineConfig() {

            auto shaderConfig = ShaderConfig{
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .swapChain = device->swapChain
            };
            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

            return PipelineConfig(shaderConfig, pipelineDesc, GetMacros());

        }

        PipelineConfig PostProcessRenderer::GetMainPipelineConfig(const Graphics::AttachmentFormats& attachmentFormats) {

            auto shaderConfig = ShaderConfig{
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = attachmentFormats
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

    }
}
