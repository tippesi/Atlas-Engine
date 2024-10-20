#include "SSGIRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"

namespace Atlas {

    namespace Renderer {

        void SSGIRenderer::Init(Graphics::GraphicsDevice *device) {

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
            temporalPipelineConfig = PipelineConfig("ssgi/temporal.csh");

            horizontalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"HORIZONTAL", "DEPTH_WEIGHT","NORMAL_WEIGHT", "BLUR_RGBA"});
            verticalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"VERTICAL", "DEPTH_WEIGHT","NORMAL_WEIGHT", "BLUR_RGBA"});

            ssUniformBuffer = Buffer::UniformBuffer(sizeof(SSUniforms));
            // If we don't set the element size to the whole thing, otherwise uniform buffer element alignment kicks in
            blurWeightsUniformBuffer = Buffer::UniformBuffer(sizeof(float) * (size_t(filterSize) + 1));

        }

        void SSGIRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto ssgi = scene->ssgi;
            if (!ssgi || !ssgi->enable) return;
            
            ivec2 res = ivec2(target->giTexture.width, target->giTexture.height);

            Graphics::Profiler::BeginQuery("Render SSGI");

            if (ssgi->halfResolution && target->GetGIResolution() == FULL_RES)
                target->SetGIResolution(HALF_RES, false);
            else if (!ssgi->halfResolution && target->GetGIResolution() != FULL_RES)
                target->SetGIResolution(FULL_RES, false);

            if (!target->giLengthTexture.IsValid())
                target->SetGIResolution(target->GetGIResolution(), false);

            if (target->historyGiTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED || 
                target->historyGiLengthTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers = {
                    {target->historyGiTexture.image, layout, access},
                    {target->historyGiLengthTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);
            }

            // Try to get a shadow map
            Ref<Lighting::Shadow> shadow = nullptr;
            auto mainLightEntity = GetMainLightEntity(scene);
            if (mainLightEntity.IsValid())
                shadow = mainLightEntity.GetComponent<LightComponent>().shadow;

            auto downsampledRT = target->GetData(target->GetGIResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetGIResolution());

            // Should be reflection resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->normalTexture;
            auto geometryNormalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;
            auto lightingTexture = &target->lightingTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = downsampledHistoryRT->normalTexture;
            auto historyGeometryNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            // Bind the geometry normal texure and depth texture
            commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 1);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 3);
            commandList->BindImage(offsetTexture->image, offsetTexture->sampler, 3, 4);
            commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 5);
            commandList->BindImage(lightingTexture->image, lightingTexture->sampler, 3, 6);

            commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 7);
            commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 8);

            Graphics::Profiler::BeginQuery("Main pass");

            {
                static int32_t frameCount = 0;

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
                    .downsampled2x = target->GetGIResolution() == RenderResolution::HALF_RES,
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


            {
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
                commandList->BindImage(geometryNormalTexture->image, geometryNormalTexture->sampler, 3, 6);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 7);

                commandList->BindImage(target->historyGiTexture.image, target->historyGiTexture.sampler, 3, 8);
                commandList->BindImage(target->historyGiLengthTexture.image, target->historyGiLengthTexture.sampler, 3, 9);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 10);
                commandList->BindImage(historyGeometryNormalTexture->image, historyGeometryNormalTexture->sampler, 3, 11);
                commandList->BindImage(historyMaterialIdxTexture->image, historyMaterialIdxTexture->sampler, 3, 12);

                int32_t resetHistory = !target->HasHistory() ? 1 : 0;
                commandList->PushConstants("constants", &resetHistory, sizeof(int32_t));

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
