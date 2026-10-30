#include "RTGIRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"
#include "loader/ImageLoader.h"

namespace Atlas {

    namespace Renderer {

        void RTGIRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            rtPipelineConfig = PipelineConfig("rtgi/rtgi.csh");
            rtShadowPipelineConfig = PipelineConfig("raytracer/traceShadow.csh");
            rtResolvePipelineConfig = PipelineConfig("rtgi/rtgiResolve.csh");
            temporalPipelineConfig = PipelineConfig("rtgi/temporal.csh");

            atrousPipelineConfig[0] = PipelineConfig("rtgi/atrous.csh", { "STEP_SIZE1" });
            atrousPipelineConfig[1] = PipelineConfig("rtgi/atrous.csh", { "STEP_SIZE2" });
            atrousPipelineConfig[2] = PipelineConfig("rtgi/atrous.csh", { "STEP_SIZE4" });

            rtUniformBuffer = Buffer::UniformBuffer(sizeof(RTUniforms));

            deferredSampleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, 2 * sizeof(vec4));
            deferredLightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4));
            deferredShadowRayBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, 2 * sizeof(vec4));
            deferredShadowRayCountBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t), 1);

        }

        void RTGIRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {
            
            auto rtgi = scene->rtgi;
            if (!rtgi || !rtgi->enable || !scene->IsRtDataValid()) return;

            if (rtgi->halfResolution && target->GetGIResolution() == FULL_RES)
                target->SetGIResolution(HALF_RES, true);
            else if (!rtgi->halfResolution && target->GetGIResolution() != FULL_RES)
                target->SetGIResolution(FULL_RES, true);

            if (!target->giMomentsTexture.IsValid())
                target->SetGIResolution(target->GetGIResolution(), true);

            helper.UpdateLights(scene, false);

            ivec2 res = target->GetRelativeResolution(target->GetGIResolution());

            Graphics::Profiler::BeginQuery("Render RTGI");

            if (target->historyGiTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED || 
                target->historyGiMomentsTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                Graphics::ImageBarrier imageBarriers[] = {
                    {target->historyGiTexture.image, layout, access},
                    {target->historyGiMomentsTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, {});
            }

            Graphics::Profiler::BeginQuery("Trace rays");

            auto mainCamera = scene->GetMainCamera();

            auto downsampledRT = target->GetData(target->GetGIResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetGIResolution());

            // Should be gi resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = rtgi->useNormalMaps ? downsampledRT->normalTexture : downsampledRT->geometryNormalTexture;
            auto geometryNormalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = rtgi->useNormalMaps ? downsampledHistoryRT->normalTexture : downsampledHistoryRT->geometryNormalTexture;
            auto historyGeometryNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            // Bind the geometry normal texure and depth texture
            commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 1);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 3);
            commandList->BindImage(offsetTexture->image, offsetTexture->sampler, 3, 4);
            commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 5);

            commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 7);
            commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 8);

            target->exposureTexture.Bind(commandList, 3, 11);

            // Cast rays and calculate radiance
            {
                static uint32_t frameCount = 0;

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                auto clouds = scene->sky.clouds;

                auto ddgiEnabled = scene->irradianceVolume && scene->irradianceVolume->enable;
                auto ddgiVisibility = ddgiEnabled && scene->irradianceVolume->visibility;
                auto cloudShadowEnabled = clouds && clouds->enable && clouds->castShadow;

                rtPipelineConfig.ManageMacro("DDGI", rtgi->ddgi && ddgiEnabled);
                rtPipelineConfig.ManageMacro("VISIBILITY_VOLUME", rtgi->ddgi && ddgiVisibility);
                rtPipelineConfig.ManageMacro("OPACITY_CHECK", rtgi->opacityCheck);
                rtPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowEnabled && scene->HasMainLight());
                rtPipelineConfig.ManageMacro("AUTO_EXPOSURE", scene->postProcessing.autoExposure.enable);

                // The hit shader has a low occupancy with software ray tracing
                auto deferredShadowRays = !device->support.hardwareRayTracing;
                rtPipelineConfig.ManageMacro("DEFERRED_SHADOW_RAYS", deferredShadowRays);

                auto pipeline = PipelineManager::GetPipeline(rtPipelineConfig);

                auto sampleCount = size_t(res.x) * size_t(res.y) * size_t(rtgi->sampleCount);
                auto lightCount = sampleCount * size_t(rtgi->lightSampleCount);
                if (deferredShadowRays) {
                    if (deferredSampleBuffer.GetElementCount() != sampleCount)
                        deferredSampleBuffer.SetSize(sampleCount);
                    if (deferredLightBuffer.GetElementCount() != lightCount) {
                        deferredLightBuffer.SetSize(lightCount);
                        deferredShadowRayBuffer.SetSize(lightCount);
                    }

                    Graphics::BufferBarrier preBufferBarriers[] = {
                        { deferredShadowRayCountBuffer.Get(), VK_ACCESS_TRANSFER_WRITE_BIT }
                    };
                    commandList->PipelineBarrier({}, preBufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT);

                    uint32_t zero = 0;
                    commandList->FillBuffer(deferredShadowRayCountBuffer.Get(), &zero);

                    Graphics::BufferBarrier postBufferBarriers[] = {
                        { deferredShadowRayCountBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT }
                    };
                    commandList->PipelineBarrier({}, postBufferBarriers, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                }

                commandList->ImageMemoryBarrier(target->giTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                helper.DispatchAndHit(scene, commandList, pipeline, ivec3(groupCount, 1),
                    [=]() {
                        commandList->BindImage(target->giTexture.image, 3, 0);

                        RTUniforms uniforms;
                        uniforms.radianceLimit = rtgi->radianceLimit / glm::max(mainCamera.exposure, 0.00001f);
                        uniforms.bias = rtgi->bias;
                        uniforms.frameSeed = frameCount++;
                        uniforms.sampleCount = rtgi->sampleCount;
                        uniforms.lightSampleCount = rtgi->lightSampleCount;
                        uniforms.textureLevel = rtgi->textureLevel;
                        uniforms.halfRes = target->GetGIResolution() == HALF_RES ? 1 : 0;
                        uniforms.resolution = res;

                        if (cloudShadowEnabled && scene->HasMainLight()) {
                            clouds->shadowTexture.Bind(commandList, 3, 9);
                        }

                        rtUniformBuffer.SetData(&uniforms, 0);
                        commandList->BindBuffer(rtUniformBuffer.Get(), 3, 10);

                        if (deferredShadowRays) {
                            commandList->BindBuffer(deferredSampleBuffer.Get(), 3, 12);
                            commandList->BindBuffer(deferredLightBuffer.Get(), 3, 13);
                            commandList->BindBuffer(deferredShadowRayBuffer.Get(), 3, 14);
                            commandList->BindBuffer(deferredShadowRayCountBuffer.Get(), 3, 15);
                        }

                    });

                if (deferredShadowRays) {
                    Graphics::Profiler::EndAndBeginQuery("Shadow rays");

                    Graphics::BufferBarrier bufferBarriers[] = {
                        { deferredSampleBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
                        { deferredLightBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
                        { deferredShadowRayBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
                        { deferredShadowRayCountBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT }
                    };
                    commandList->PipelineBarrier({}, bufferBarriers);

                    auto shadowPipeline = PipelineManager::GetPipeline(rtShadowPipelineConfig);
                    commandList->BindPipeline(shadowPipeline);

                    commandList->BindBuffer(deferredLightBuffer.Get(), 3, 13);
                    commandList->BindBuffer(deferredShadowRayBuffer.Get(), 3, 14);
                    commandList->BindBuffer(deferredShadowRayCountBuffer.Get(), 3, 15);

                    const uint32_t shadowGroupSize = 32;
                    auto shadowGroupCount = uint32_t(lightCount) / shadowGroupSize;
                    shadowGroupCount += (uint32_t(lightCount) % shadowGroupSize == 0) ? 0 : 1;

                    commandList->Dispatch(shadowGroupCount, 1, 1);

                    commandList->BufferMemoryBarrier(deferredLightBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);

                    Graphics::Profiler::EndAndBeginQuery("Resolve");

                    auto resolvePipeline = PipelineManager::GetPipeline(rtResolvePipelineConfig);
                    commandList->BindPipeline(resolvePipeline);

                    commandList->BindImage(target->giTexture.image, 3, 0);
                    commandList->BindBuffer(deferredSampleBuffer.Get(), 3, 12);
                    commandList->BindBuffer(deferredLightBuffer.Get(), 3, 13);

                    ResolveConstants constants = {
                        .resolution = res,
                        .sampleCount = rtgi->sampleCount,
                        .lightSampleCount = rtgi->lightSampleCount
                    };
                    commandList->PushConstants("constants", &constants);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                }

                commandList->ImageMemoryBarrier(target->giTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }

            Graphics::Profiler::EndAndBeginQuery("Temporal filter");

            {
                ivec2 groupCount = ivec2(res.x / 16, res.y / 16);
                groupCount.x += ((groupCount.x * 16 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == res.y) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(temporalPipelineConfig);
                commandList->BindPipeline(pipeline);

                TemporalConstants constants = {
                    .temporalWeight = rtgi->temporalWeight,
                    .historyClipMax = rtgi->historyClipMax,
                    .currentClipFactor = rtgi->currentClipFactor,
                    .resetHistory = !target->HasHistory() ? 1 : 0
                };

                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->swapGiTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->giMomentsTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(target->swapGiTexture.image, 3, 0);
                commandList->BindImage(target->giMomentsTexture.image, 3, 1);

                commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 2);
                commandList->BindImage(velocityTexture->image, velocityTexture->sampler, 3, 3);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);
                commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 5);
                commandList->BindImage(geometryNormalTexture->image, geometryNormalTexture->sampler, 3, 6);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 7);

                commandList->BindImage(target->historyGiTexture.image, target->historyGiTexture.sampler, 3, 8);
                commandList->BindImage(target->historyGiMomentsTexture.image, target->historyGiMomentsTexture.sampler, 3, 9);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 10);
                commandList->BindImage(historyGeometryNormalTexture->image, historyGeometryNormalTexture->sampler, 3, 11);
                commandList->BindImage(historyMaterialIdxTexture->image, historyMaterialIdxTexture->sampler, 3, 12);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            // Need barriers for all four images
            Graphics::ImageBarrier preImageBarriers[] = {
                {target->swapGiTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {target->giMomentsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {target->historyGiTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                {target->historyGiMomentsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            commandList->PipelineBarrier(preImageBarriers, {},
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->CopyImage(target->swapGiTexture.image, target->historyGiTexture.image);
            commandList->CopyImage(target->giMomentsTexture.image, target->historyGiMomentsTexture.image);

            // Need barriers for all four images
            Graphics::ImageBarrier postImageBarriers[] = {
                {target->swapGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->giMomentsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->historyGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->historyGiMomentsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(postImageBarriers, {},
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            Graphics::Profiler::EndAndBeginQuery("Spatial filter");

            {
                ivec2 groupCount = ivec2(res.x / 16, res.y / 16);
                groupCount.x += ((groupCount.x * 16 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == res.y) ? 0 : 1);

                bool pingpong = false;

                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 3);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 4);           
                commandList->BindImage(target->historyGiMomentsTexture.image, target->historyGiMomentsTexture.sampler, 3, 5);           

                for (int32_t i = 0; i < 3; i++) {
                    Graphics::Profiler::BeginQuery("Subpass " + std::to_string(i));

                    auto pipeline = PipelineManager::GetPipeline(atrousPipelineConfig[i]);
                    commandList->BindPipeline(pipeline);

                    AtrousConstants constants = {
                        .stepSize = 1 << i,
                        .strength = rtgi->spatialFilterStrength
                    };
                    commandList->PushConstants("constants", &constants);

                    if (pingpong) {
                        Graphics::ImageBarrier imageBarriers[] = {
                            {target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                            {target->swapGiTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        };
                        commandList->BindImage(target->swapGiTexture.image, 3, 0);
                        commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 1);
                        commandList->PipelineBarrier(imageBarriers, {});
                    }
                    else {
                        Graphics::ImageBarrier imageBarriers[] = {
                            {target->swapGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                            {target->giTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        };
                        commandList->BindImage(target->giTexture.image, 3, 0);
                        commandList->BindImage(target->swapGiTexture.image, target->swapGiTexture.sampler, 3, 1);
                        commandList->PipelineBarrier(imageBarriers, {});
                    }

                    pingpong = !pingpong;

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                    Graphics::Profiler::EndQuery();
                }

                // Transition to final layout, the loop won't do that
                commandList->ImageMemoryBarrier(target->giTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }
            
            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

    }

}