#include "RTGIRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"
#include "loader/ImageLoader.h"

namespace Atlas {

    namespace Renderer {

        void RTGIRenderer::Init(Graphics::GraphicsDevice* device) {
            
            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            rtPipelineConfig = PipelineConfig("rtgi/rtgi.csh");
            temporalPipelineConfig = PipelineConfig("rtgi/temporal.csh");

            atrousPipelineConfig[0] = PipelineConfig("rtgi/atrous.csh", { "STEP_SIZE1" });
            atrousPipelineConfig[1] = PipelineConfig("rtgi/atrous.csh", { "STEP_SIZE2" });
            atrousPipelineConfig[2] = PipelineConfig("rtgi/atrous.csh", { "STEP_SIZE4" });

            rtUniformBuffer = Buffer::UniformBuffer(sizeof(RTUniforms));

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

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

            if (!target->HasHistory()) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers = {
                    {target->historyGiTexture.image, layout, access},
                    {target->historyGiMomentsTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);
            }

            Graphics::Profiler::BeginQuery("Trace rays");

            // Try to get a shadow map
            Ref<Lighting::Shadow> shadow = nullptr;
            auto mainLightEntity = GetMainLightEntity(scene);
            if (mainLightEntity.IsValid())
                shadow = mainLightEntity.GetComponent<LightComponent>().shadow;

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

            // Cast rays and calculate radiance
            {
                static uint32_t frameCount = 0;

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                auto ddgiEnabled = scene->irradianceVolume && scene->irradianceVolume->enable;
                auto ddgiVisibility = ddgiEnabled && scene->irradianceVolume->visibility;

                rtPipelineConfig.ManageMacro("USE_SHADOW_MAP", rtgi->useShadowMap && shadow);
                rtPipelineConfig.ManageMacro("DDGI", rtgi->ddgi && ddgiEnabled);
                rtPipelineConfig.ManageMacro("DDGI_VISIBILITY", rtgi->ddgi && ddgiVisibility);
                rtPipelineConfig.ManageMacro("OPACITY_CHECK", rtgi->opacityCheck);

                auto pipeline = PipelineManager::GetPipeline(rtPipelineConfig);

                commandList->ImageMemoryBarrier(target->giTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                helper.DispatchAndHit(scene, commandList, pipeline, ivec3(groupCount, 1),
                    [=]() {
                        commandList->BindImage(target->giTexture.image, 3, 0);

                        RTUniforms uniforms;
                        uniforms.radianceLimit = rtgi->radianceLimit / glm::max(mainCamera.exposure, 0.00001f);
                        uniforms.bias = rtgi->bias;
                        uniforms.frameSeed = frameCount++;                        
                        uniforms.textureLevel = rtgi->textureLevel;
                        uniforms.halfRes = target->GetGIResolution() == HALF_RES ? 1 : 0;
                        uniforms.resolution = res;

                        if (shadow && rtgi->useShadowMap) {
                            auto& shadowUniform = uniforms.shadow;
                            shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                            shadowUniform.bias = shadow->bias;
                            shadowUniform.edgeSoftness = shadow->edgeSoftness;
                            shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                            shadowUniform.cascadeCount = shadow->viewCount;
                            shadowUniform.resolution = vec2(shadow->resolution);

                            commandList->BindImage(shadow->maps.image, shadowSampler, 3, 6);

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
                                        cascade->viewMatrix;
                                    shadowUniform.cascades[i].texelSize = texelSize;
                                }
                                else {
                                    auto cascade = &shadow->views[componentCount - 1];
                                    shadowUniform.cascades[i].distance = cascade->farDistance;
                                }
                            }
                        }
                        rtUniformBuffer.SetData(&uniforms, 0);
                        commandList->BindBuffer(rtUniformBuffer.Get(), 3, 9);

                    });

                commandList->ImageMemoryBarrier(target->giTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }

            Graphics::Profiler::EndAndBeginQuery("Temporal filter");

            {
                std::vector<Graphics::ImageBarrier> imageBarriers;
                std::vector<Graphics::BufferBarrier> bufferBarriers;

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

                imageBarriers = {
                    {target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->swapGiTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->giMomentsTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

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

            std::vector<Graphics::ImageBarrier> imageBarriers;
            std::vector<Graphics::BufferBarrier> bufferBarriers;

            // Need barriers for all four images
            imageBarriers = {
                {target->swapGiTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {target->giMomentsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {target->historyGiTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                {target->historyGiMomentsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->CopyImage(target->swapGiTexture.image, target->historyGiTexture.image);
            commandList->CopyImage(target->giMomentsTexture.image, target->historyGiMomentsTexture.image);

            // Need barriers for all four images
            imageBarriers = {
                {target->swapGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->giMomentsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->historyGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->historyGiMomentsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers,
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
                        imageBarriers = {
                            {target->giTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                            {target->swapGiTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        };
                        commandList->BindImage(target->swapGiTexture.image, 3, 0);
                        commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 1);
                    }
                    else {
                        imageBarriers = {
                            {target->swapGiTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                            {target->giTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        };
                        commandList->BindImage(target->giTexture.image, 3, 0);
                        commandList->BindImage(target->swapGiTexture.image, target->swapGiTexture.sampler, 3, 1);
                    }
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);

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