#include "AORenderer.h"

#include "Clock.h"

namespace Atlas {

    namespace Renderer {

        void AORenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            blurFilter.CalculateGaussianFilter(float(filterSize) / 3.0f, filterSize);

            std::vector<float> blurKernelWeights;
            std::vector<float> blurKernelOffsets;
            blurFilter.GetLinearized(&blurKernelWeights, &blurKernelOffsets, false);

            auto mean = (blurKernelWeights.size() - 1) / 2;
            blurKernelWeights = std::vector<float>(blurKernelWeights.begin() + mean, blurKernelWeights.end());
            blurKernelOffsets = std::vector<float>(blurKernelOffsets.begin() + mean, blurKernelOffsets.end());

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            ssaoPipelineConfig = PipelineConfig("ao/ssao.csh");
            rtaoPipelineConfig = PipelineConfig("ao/rtao.csh");
            temporalPipelineConfig = PipelineConfig("ao/temporal.csh");

            horizontalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"HORIZONTAL", "DEPTH_WEIGHT","NORMAL_WEIGHT" });
            verticalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"VERTICAL", "DEPTH_WEIGHT","NORMAL_WEIGHT" });

            rtUniformBuffer = Buffer::UniformBuffer(sizeof(RTUniforms));
            ssUniformBuffer = Buffer::UniformBuffer(sizeof(SSUniforms));
            // If we don't set the element size to the whole thing, otherwise uniform buffer element alignment kicks in
            ssSamplesUniformBuffer = Buffer::UniformBuffer(sizeof(vec4) * 64);
            blurWeightsUniformBuffer = Buffer::Buffer(Buffer::BufferUsageBits::UniformBufferBit,
                sizeof(float) * (size_t(filterSize) + 1), 1, blurKernelWeights.data());

        }

        void AORenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            static int32_t frameCount = 0;

            auto ao = scene->ao;
            if (!ao || !ao->enable) return;

            if (!scene->IsRtDataValid() && ao->rt) return;

            ivec2 res = ivec2(target->aoTexture.width, target->aoTexture.height);

            Graphics::Profiler::BeginQuery("Render AO");

            if (!target->HasHistory()) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

                Graphics::ImageBarrier imageBarriers[] = {
                    {target->historyAoTexture.image, layout, access},
                    {target->historyAoLengthTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, {});
            }

            auto downsampledRT = target->GetData(target->GetAOResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetAOResolution());

            // Should be AO resolution and we assume that all are shader read optimal
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            // Calculate RTAO
            if (ao->rt) {
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

                helper.DispatchAndHit(scene, commandList, pipeline, ivec3(groupCount.x * groupCount.y, 1, 1),
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

            }
            else {
                Graphics::Profiler::BeginQuery("Main pass");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
                groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(ssaoPipelineConfig);
                commandList->BindPipeline(pipeline);

                auto uniforms = SSUniforms {
                    .radius = ao->radius,
                    .sampleCount = ao->sampleCount,
                    .frameCount = frameCount++
                };
                ssUniformBuffer.SetData(&uniforms, 0);
                ssSamplesUniformBuffer.SetData(&ao->samples[0], 0, ao->samples.size() * sizeof(vec4));

                commandList->BindImage(target->aoTexture.image, 3, 0);

                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 1);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
                commandList->BindImage(ao->noiseTexture.image, ao->noiseTexture.sampler, 3, 3);

                commandList->BindBuffer(ssUniformBuffer.Get(), 3, 4);
                commandList->BindBuffer(ssSamplesUniformBuffer.Get(), 3, 5);

                commandList->ImageMemoryBarrier(target->aoTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(target->aoTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }


            if (ao->rt) {
                Graphics::Profiler::EndAndBeginQuery("Temporal filter");

                ivec2 groupCount = ivec2(res.x / 16, res.y / 16);
                groupCount.x += ((groupCount.x * 16 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == res.y) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(temporalPipelineConfig);
                commandList->BindPipeline(pipeline);

                Graphics::ImageBarrier imageBarriers[] = {
                    {target->aoTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->aoLengthTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(target->aoTexture.image, 3, 0);
                commandList->BindImage(target->aoLengthTexture.image, 3, 1);

                commandList->BindImage(target->swapAoTexture.image, target->swapAoTexture.sampler, 3, 2);
                commandList->BindImage(velocityTexture->image, velocityTexture->sampler, 3, 3);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);
                commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 5);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 6);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 7);

                commandList->BindImage(target->historyAoTexture.image, target->historyAoTexture.sampler, 3, 8);
                commandList->BindImage(target->historyAoLengthTexture.image, target->historyAoLengthTexture.sampler, 3, 9);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 10);
                commandList->BindImage(historyNormalTexture->image, historyNormalTexture->sampler, 3, 11);
                commandList->BindImage(historyMaterialIdxTexture->image, historyMaterialIdxTexture->sampler, 3, 12);

                int32_t resetHistory = !target->HasHistory() ? 1 : 0;
                commandList->PushConstants("constants", &resetHistory, sizeof(int32_t));

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                // Need barriers for all four images
                Graphics::ImageBarrier preCopyImageBarriers[] = {
                    {target->aoTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {target->aoLengthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {target->historyAoTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    {target->historyAoLengthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                };
                commandList->PipelineBarrier(preCopyImageBarriers, {},
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                commandList->CopyImage(target->aoTexture.image, target->historyAoTexture.image);
                commandList->CopyImage(target->aoLengthTexture.image, target->historyAoLengthTexture.image);

                // Need barriers for all four images
                Graphics::ImageBarrier postCopyImageBarriers[] = {
                    {target->aoTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->aoLengthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyAoTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyAoLengthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(postCopyImageBarriers, {},
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }

            {
                Graphics::Profiler::EndAndBeginQuery("Blur");

                const int32_t groupSize = 256;

                auto kernelSize = int32_t(filterSize);

                auto horizontalBlurPipeline = PipelineManager::GetPipeline(horizontalBlurPipelineConfig);
                auto verticalBlurPipeline = PipelineManager::GetPipeline(verticalBlurPipelineConfig);

                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 3);
                commandList->BindBuffer(blurWeightsUniformBuffer.Get(), 3, 4);

                for (int32_t i = 0; i < 3; i++) {
                    ivec2 groupCount = ivec2(res.x / groupSize, res.y);
                    groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);

                    Graphics::ImageBarrier horizImageBarriers[] = {
                       {target->aoTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                       {target->swapAoTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(horizImageBarriers, {});

                    commandList->BindPipeline(horizontalBlurPipeline);
                    commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                    commandList->BindImage(target->swapAoTexture.image, 3, 0);
                    commandList->BindImage(target->aoTexture.image, target->aoTexture.sampler, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);

                    groupCount = ivec2(res.x, res.y / groupSize);
                    groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                    Graphics::ImageBarrier vertImageBarriers[] = {
                        {target->swapAoTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {target->aoTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(vertImageBarriers, {});

                    commandList->BindPipeline(verticalBlurPipeline);
                    commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                    commandList->BindImage(target->aoTexture.image, 3, 0);
                    commandList->BindImage(target->swapAoTexture.image, target->swapAoTexture.sampler, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                }
            }

            commandList->ImageMemoryBarrier(target->aoTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);
            
            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

    }

}
