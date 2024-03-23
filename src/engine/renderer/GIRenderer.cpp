#include "GIRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"

namespace Atlas {

    namespace Renderer {

        void GIRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            const int32_t filterSize = 3;
            blurFilter.CalculateBoxFilter(filterSize);

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            ssgiPipelineConfig = PipelineConfig("ssgi/ssgi.csh");
            //rtaoPipelineConfig = PipelineConfig("ao/rtao.csh");
            temporalPipelineConfig = PipelineConfig("ssgi/temporal.csh");

            horizontalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"HORIZONTAL", "DEPTH_WEIGHT","NORMAL_WEIGHT", "BLUR_RGBA"});
            verticalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"VERTICAL", "DEPTH_WEIGHT","NORMAL_WEIGHT", "BLUR_RGBA"});

            rtUniformBuffer = Buffer::UniformBuffer(sizeof(RTUniforms));
            ssUniformBuffer = Buffer::UniformBuffer(sizeof(SSUniforms));
            // If we don't set the element size to the whole thing, otherwise uniform buffer element alignment kicks in
            blurWeightsUniformBuffer = Buffer::UniformBuffer(sizeof(float) * (size_t(filterSize) + 1));

        }

        void GIRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto ssgi = scene->ssgi;
            if (!ssgi || !ssgi->enable) return;

            if (!scene->IsRtDataValid() && ssgi->rt) return;
            
            ivec2 res = ivec2(target->giTexture.width, target->giTexture.height);

            Graphics::Profiler::BeginQuery("Render SSGI");

            auto downsampledRT = target->GetData(target->GetGIResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetGIResolution());

            // Should be reflection resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;
            auto lightingTexture = &target->lightingTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            // Bind the geometry normal texure and depth texture
            commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 1);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 3);
            commandList->BindImage(offsetTexture->image, offsetTexture->sampler, 3, 4);
            commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 5);
            commandList->BindImage(lightingTexture->image, lightingTexture->sampler, 3, 6);

            commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 7);
            commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 8);

            // Calculate RTAO
            if (ssgi->rt) {
                /*
                Graphics::Profiler::BeginQuery("Trace rays/calculate ao");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                rtaoPipelineConfig.ManageMacro("OPACITY_CHECK", ao->opacityCheck);

                auto pipeline = PipelineManager::GetPipeline(rtaoPipelineConfig);

                auto uniforms = RTUniforms {
                    .radius = ao->radius,
                    .frameSeed = frameCount++,
                };
                rtUniformBuffer.SetData(&uniforms, 0);

                commandList->ImageMemoryBarrier(target->swapAoTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                helper.DispatchAndHit(commandList, pipeline, ivec3(groupCount.x * groupCount.y, 1, 1),
                    [=]() {
                        commandList->BindImage(target->swapAoTexture.image, 3, 0);

                        commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 1);
                        commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
                        commandList->BindImage(offsetTexture->image, offsetTexture->sampler, 3, 3);

                        commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 4);
                        commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 5);

                        commandList->BindBuffer(rtUniformBuffer.Get(), 3, 6);
                    });

                commandList->ImageMemoryBarrier(target->swapAoTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                */

            }
            else {
                static int32_t frameCount = 0;

                Graphics::Profiler::BeginQuery("Main pass");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
                groupCount.y += ((res.y % 4 == 0) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(ssgiPipelineConfig);
                commandList->BindPipeline(pipeline);

                auto uniforms = SSUniforms {
                    .radianceLimit = ssgi->irradianceLimit,
                    .frameSeed = uint32_t(frameCount++),
                    .radius = ssgi->radius,
                    .rayCount = uint32_t(ssgi->rayCount),
                    .sampleCount = uint32_t(ssgi->sampleCount),
                };
                ssUniformBuffer.SetData(&uniforms, 0);

                commandList->BindImage(target->swapGiTexture.image, 3, 0);
                commandList->BindBuffer(ssUniformBuffer.Get(), 3, 9);

                commandList->ImageMemoryBarrier(target->swapGiTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(target->swapGiTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }


            if (true) {
                Graphics::Profiler::EndAndBeginQuery("Temporal filter");

                std::vector<Graphics::ImageBarrier> imageBarriers;
                std::vector<Graphics::BufferBarrier> bufferBarriers;

                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(temporalPipelineConfig);
                commandList->BindPipeline(pipeline);

                imageBarriers = {
                    {target->giTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->giLengthTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->historyGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyGiLengthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                commandList->BindImage(target->giTexture.image, 3, 0);
                commandList->BindImage(target->giLengthTexture.image, 3, 1);

                commandList->BindImage(target->swapGiTexture.image, target->swapGiTexture.sampler, 3, 2);
                commandList->BindImage(velocityTexture->image, velocityTexture->sampler, 3, 3);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);
                commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 5);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 6);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 7);

                commandList->BindImage(target->historyGiTexture.image, target->historyGiTexture.sampler, 3, 8);
                commandList->BindImage(target->historyGiLengthTexture.image, target->historyGiLengthTexture.sampler, 3, 9);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 10);
                commandList->BindImage(historyNormalTexture->image, historyNormalTexture->sampler, 3, 11);
                commandList->BindImage(historyMaterialIdxTexture->image, historyMaterialIdxTexture->sampler, 3, 12);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                // Need barriers for all four images
                imageBarriers = {
                    {target->giTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {target->giLengthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {target->historyGiTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    {target->historyGiLengthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                commandList->CopyImage(target->giTexture.image, target->historyGiTexture.image);
                commandList->CopyImage(target->giLengthTexture.image, target->historyGiLengthTexture.image);

                // Need barriers for all four images
                imageBarriers = {
                    {target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->giLengthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyGiLengthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                
            }

            {
                Graphics::Profiler::EndAndBeginQuery("Blur");

                const int32_t groupSize = 256;

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets, false);

                auto mean = (kernelWeights.size() - 1) / 2;
                kernelWeights = std::vector<float>(kernelWeights.begin() + mean, kernelWeights.end());
                kernelOffsets = std::vector<float>(kernelOffsets.begin() + mean, kernelOffsets.end());

                auto kernelSize = int32_t(kernelWeights.size() - 1);

                auto horizontalBlurPipeline = PipelineManager::GetPipeline(horizontalBlurPipelineConfig);
                auto verticalBlurPipeline = PipelineManager::GetPipeline(verticalBlurPipelineConfig);

                blurWeightsUniformBuffer.SetData(kernelWeights.data(), 0);

                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 3);
                commandList->BindBuffer(blurWeightsUniformBuffer.Get(), 3, 4);

                std::vector<Graphics::BufferBarrier> bufferBarriers;

                for (int32_t i = 0; i < 3; i++) {
                    ivec2 groupCount = ivec2(res.x / groupSize, res.y);
                    groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);

                    std::vector<Graphics::ImageBarrier> imageBarriers = {
                        {target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {target->swapGiTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                    commandList->BindPipeline(horizontalBlurPipeline);
                    commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                    commandList->BindImage(target->swapGiTexture.image, 3, 0);
                    commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);

                    groupCount = ivec2(res.x, res.y / groupSize);
                    groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                    imageBarriers = {
                        {target->swapGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {target->giTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                    commandList->BindPipeline(verticalBlurPipeline);
                    commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                    commandList->BindImage(target->giTexture.image, 3, 0);
                    commandList->BindImage(target->swapGiTexture.image, target->swapGiTexture.sampler, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                }
            }

            commandList->ImageMemoryBarrier(target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

    }

}
