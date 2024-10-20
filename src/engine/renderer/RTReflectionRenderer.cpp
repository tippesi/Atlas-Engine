#include "RTReflectionRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"
#include "loader/ImageLoader.h"

namespace Atlas {

    namespace Renderer {

        void RTReflectionRenderer::Init(Graphics::GraphicsDevice* device) {
            
            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            ssrPipelineConfig = PipelineConfig("reflection/ssr.csh");
            rtrPipelineConfig = PipelineConfig("reflection/rtreflection.csh");
            upsamplePipelineConfig = PipelineConfig("reflection/upsample.csh");
            temporalPipelineConfig = PipelineConfig("reflection/temporal.csh");

            atrousPipelineConfig[0] = PipelineConfig("reflection/atrous.csh", { "STEP_SIZE1" });
            atrousPipelineConfig[1] = PipelineConfig("reflection/atrous.csh", { "STEP_SIZE2" });
            atrousPipelineConfig[2] = PipelineConfig("reflection/atrous.csh", { "STEP_SIZE4" });

            rtrUniformBuffer = Buffer::UniformBuffer(sizeof(RTRUniforms));

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void RTReflectionRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {
            
            auto reflection = scene->reflection;
            if (!reflection || !reflection->enable || !scene->IsRtDataValid()) return;

            if (reflection->halfResolution && !reflection->upsampleBeforeFiltering && target->GetReflectionResolution() == FULL_RES)
                target->SetReflectionResolution(HALF_RES);
            else if ((!reflection->halfResolution || reflection->upsampleBeforeFiltering) && target->GetReflectionResolution() != FULL_RES)
                target->SetReflectionResolution(FULL_RES);

            helper.UpdateLights(scene, false);

            ivec2 rayRes = !reflection->halfResolution ? target->GetRelativeResolution(FULL_RES) : target->GetRelativeResolution(HALF_RES);
            ivec2 res = target->GetRelativeResolution(target->GetReflectionResolution());

            Graphics::Profiler::BeginQuery("Render RT Reflections");

            if (target->historyReflectionTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED || 
                target->historyReflectionMomentsTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;
                Graphics::ImageBarrier imageBarriers[] = {
                    {target->historyReflectionTexture.image, layout, access},
                    {target->historyReflectionMomentsTexture.image, layout, access},
                };
                commandList->PipelineBarrier(imageBarriers, {});
            }

           

            // Try to get a shadow map
            Ref<Lighting::Shadow> shadow = nullptr;
            auto mainLightEntity = GetMainLightEntity(scene);
            if (mainLightEntity.IsValid())
                shadow = mainLightEntity.GetComponent<LightComponent>().shadow;

            auto downsampledRT = target->GetData(!reflection->halfResolution ? FULL_RES : HALF_RES);

            // Should be reflection resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = reflection->useNormalMaps ? downsampledRT->normalTexture : downsampledRT->geometryNormalTexture;
            auto geometryNormalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;
            auto lightingTexture = &target->lightingTexture;

            // Bind the geometry normal texure and depth texture
            commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 1);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 3);
            commandList->BindImage(offsetTexture->image, offsetTexture->sampler, 3, 4);
            commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 5);

            commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 7);
            commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 8);

            commandList->BindImage(lightingTexture->image, lightingTexture->sampler, 3, 9);

            Texture::Texture2D* reflectionTexture = reflection->upsampleBeforeFiltering ? &target->swapReflectionTexture : &target->reflectionTexture;
            Texture::Texture2D* swapReflectionTexture = reflection->upsampleBeforeFiltering ? &target->reflectionTexture : &target->swapReflectionTexture;

            static uint32_t frameCount = 0;

            RTRUniforms uniforms;
            uniforms.radianceLimit = reflection->radianceLimit;
            uniforms.bias = reflection->bias;
            uniforms.roughnessCutoff = reflection->roughnessCutoff;
            uniforms.frameSeed = frameCount++;
            uniforms.sampleCount = reflection->sampleCount;
            uniforms.lightSampleCount = reflection->lightSampleCount;
            uniforms.textureLevel = reflection->textureLevel;
            uniforms.halfRes = target->GetReflectionResolution() == HALF_RES ? 1 : 0;
            uniforms.resolution = rayRes;

            if (shadow && reflection->useShadowMap) {
                auto& shadowUniform = uniforms.shadow;
                shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                shadowUniform.bias = shadow->bias;
                shadowUniform.edgeSoftness = shadow->edgeSoftness;
                shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                shadowUniform.cascadeCount = shadow->viewCount;
                shadowUniform.resolution = vec2(shadow->resolution);

                commandList->BindImage(shadow->maps->image, shadowSampler, 3, 6);

                auto componentCount = shadow->viewCount;
                for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                    if (i < componentCount) {
                        auto cascade = &shadow->views[i];
                        auto frustum = Volume::Frustum(cascade->frustumMatrix);
                        auto corners = frustum.GetCorners();
                        auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                            abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                        shadowUniform.cascades[i].cascadeSpace = glm::transpose(cascade->projectionMatrix *
                            cascade->viewMatrix);
                        shadowUniform.cascades[i].texelSize = texelSize;
                    }
                    else {
                        auto cascade = &shadow->views[componentCount - 1];
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                    }
                }
            }
            rtrUniformBuffer.SetData(&uniforms, 0);

            // Screen space reflections
            {
                Graphics::Profiler::BeginQuery("SSR");                

                ivec2 groupCount = ivec2(rayRes.x / 8, rayRes.y / 8);
                groupCount.x += ((groupCount.x * 8 == rayRes.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == rayRes.y) ? 0 : 1);

                auto ddgiEnabled = scene->irradianceVolume && scene->irradianceVolume->enable;
                auto ddgiVisibility = ddgiEnabled && scene->irradianceVolume->visibility;

                ssrPipelineConfig.ManageMacro("USE_SHADOW_MAP", reflection->useShadowMap && shadow);
                ssrPipelineConfig.ManageMacro("DDGI", reflection->ddgi && ddgiEnabled);
                ssrPipelineConfig.ManageMacro("DDGI_VISIBILITY", reflection->ddgi && ddgiVisibility);
                ssrPipelineConfig.ManageMacro("OPACITY_CHECK", reflection->opacityCheck);

                auto pipeline = PipelineManager::GetPipeline(ssrPipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->ImageMemoryBarrier(reflectionTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                commandList->BindImage(reflectionTexture->image, 3, 0);

                commandList->BindBuffer(rtrUniformBuffer.Get(), 3, 10);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::BeginQuery("Trace rays");

            // Cast rays and calculate radiance
            {
                ivec2 groupCount = ivec2(rayRes.x / 8, rayRes.y / 4);
                groupCount.x += ((groupCount.x * 8 == rayRes.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == rayRes.y) ? 0 : 1);

                auto ddgiEnabled = scene->irradianceVolume && scene->irradianceVolume->enable;
                auto ddgiVisibility = ddgiEnabled && scene->irradianceVolume->visibility;

                rtrPipelineConfig.ManageMacro("USE_SHADOW_MAP", reflection->useShadowMap && shadow);
                rtrPipelineConfig.ManageMacro("DDGI", reflection->ddgi && ddgiEnabled);
                rtrPipelineConfig.ManageMacro("DDGI_VISIBILITY", reflection->ddgi && ddgiVisibility);
                rtrPipelineConfig.ManageMacro("OPACITY_CHECK", reflection->opacityCheck);

                auto pipeline = PipelineManager::GetPipeline(rtrPipelineConfig);

                commandList->ImageMemoryBarrier(reflectionTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                helper.DispatchAndHit(scene, commandList, pipeline, ivec3(groupCount, 1),
                    [=]() {
                        commandList->BindImage(reflectionTexture->image, 3, 0);
                        commandList->BindBuffer(rtrUniformBuffer.Get(), 3, 9);
                    });

                commandList->ImageMemoryBarrier(reflectionTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }

            if (reflection->upsampleBeforeFiltering) {
                Graphics::Profiler::EndAndBeginQuery("Upscaling");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(upsamplePipelineConfig);
                commandList->BindPipeline(pipeline);

                Graphics::ImageBarrier imageBarriers[] = {
                    {reflectionTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {swapReflectionTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(swapReflectionTexture->image, 3, 0);

                commandList->BindImage(reflectionTexture->image, reflectionTexture->sampler, 3, 1);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 3);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                std::swap(reflectionTexture, swapReflectionTexture);
            }

            // Now after a potential upsampling get the actual data for the filtering
            downsampledRT = target->GetData(target->GetReflectionResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetReflectionResolution());

            // Should be reflection resolution
            depthTexture = downsampledRT->depthTexture;
            normalTexture = reflection->useNormalMaps ? downsampledRT->normalTexture : downsampledRT->geometryNormalTexture;
            geometryNormalTexture = downsampledRT->geometryNormalTexture;
            roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            offsetTexture = downsampledRT->offsetTexture;
            velocityTexture = downsampledRT->velocityTexture;
            materialIdxTexture = downsampledRT->materialIdxTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = reflection->useNormalMaps ? downsampledHistoryRT->normalTexture : downsampledHistoryRT->geometryNormalTexture;
            auto historyGeometryNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            Graphics::Profiler::EndAndBeginQuery("Temporal filter");

            {
                ivec2 groupCount = ivec2(res.x / 16, res.y / 16);
                groupCount.x += ((groupCount.x * 16 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == res.y) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(temporalPipelineConfig);
                commandList->BindPipeline(pipeline);

                TemporalConstants constants = {
                    .temporalWeight = reflection->temporalWeight,
                    .historyClipMax = reflection->historyClipMax,
                    .currentClipFactor = reflection->currentClipFactor,
                    .resetHistory = !target->HasHistory() ? 1 : 0
                };

                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {reflectionTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {swapReflectionTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->reflectionMomentsTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(swapReflectionTexture->image, 3, 0);
                commandList->BindImage(target->reflectionMomentsTexture.image, 3, 1);

                commandList->BindImage(reflectionTexture->image, reflectionTexture->sampler, 3, 2);
                commandList->BindImage(velocityTexture->image, velocityTexture->sampler, 3, 3);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);
                commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 5);
                commandList->BindImage(geometryNormalTexture->image, geometryNormalTexture->sampler, 3, 6);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 7);

                commandList->BindImage(target->historyReflectionTexture.image, target->historyReflectionTexture.sampler, 3, 8);
                commandList->BindImage(target->historyReflectionMomentsTexture.image, target->historyReflectionMomentsTexture.sampler, 3, 9);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 10);
                commandList->BindImage(historyGeometryNormalTexture->image, historyGeometryNormalTexture->sampler, 3, 11);
                commandList->BindImage(historyMaterialIdxTexture->image, historyMaterialIdxTexture->sampler, 3, 12);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            // Need barriers for all four images
            Graphics::ImageBarrier preImageBarriers[] = {
                {swapReflectionTexture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {target->reflectionMomentsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                {target->historyReflectionTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                {target->historyReflectionMomentsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
            };
            commandList->PipelineBarrier(preImageBarriers, {},
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->CopyImage(swapReflectionTexture->image, target->historyReflectionTexture.image);
            commandList->CopyImage(target->reflectionMomentsTexture.image, target->historyReflectionMomentsTexture.image);

            // Need barriers for all four images
            Graphics::ImageBarrier postImageBarriers[] = {
                {swapReflectionTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->reflectionMomentsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->historyReflectionTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->historyReflectionMomentsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
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
                commandList->BindImage(roughnessTexture->image, roughnessTexture->sampler, 3, 4);
                commandList->BindImage(materialIdxTexture->image, materialIdxTexture->sampler, 3, 5);                

                for (int32_t i = 0; i < 3; i++) {
                    Graphics::Profiler::BeginQuery("Subpass " + std::to_string(i));

                    auto pipeline = PipelineManager::GetPipeline(atrousPipelineConfig[i]);
                    commandList->BindPipeline(pipeline);

                    AtrousConstants constants = {
                        .stepSize = 1 << i,
                        .strength = reflection->spatialFilterStrength
                    };
                    commandList->PushConstants("constants", &constants);

                    if (pingpong) {
                        Graphics::ImageBarrier imageBarriers[] = {
                            {reflectionTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                            {swapReflectionTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        };
                        commandList->BindImage(swapReflectionTexture->image, 3, 0);
                        commandList->BindImage(reflectionTexture->image, reflectionTexture->sampler, 3, 1);
                        commandList->PipelineBarrier(imageBarriers, {});
                    }
                    else {
                        Graphics::ImageBarrier imageBarriers[] = {
                            {swapReflectionTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                            {reflectionTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                        };
                        commandList->BindImage(reflectionTexture->image, 3, 0);
                        commandList->BindImage(swapReflectionTexture->image, swapReflectionTexture->sampler, 3, 1);
                        commandList->PipelineBarrier(imageBarriers, {});
                    }

                    pingpong = !pingpong;

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                    Graphics::Profiler::EndQuery();
                }

                // Transition to final layout, the loop won't do that
                commandList->ImageMemoryBarrier(target->reflectionTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }
            
            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

    }

}