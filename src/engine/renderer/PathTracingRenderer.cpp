#include "PathTracingRenderer.h"
#include "../Log.h"
#include "../Clock.h"
#include "../common/Packing.h"
#include "../common/RandomHelper.h"
#include "../pipeline/PipelineManager.h"

#include "../volume/BVH.h"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

    namespace Renderer {

        void PathTracingRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            // helper.stochasticLightSelection = true;

            rayGenPipelineConfig = PipelineConfig("pathtracer/rayGen.csh");
            rayHitPipelineConfig = PipelineConfig("pathtracer/rayHit.csh");

            rayGenUniformBuffer = Buffer::UniformBuffer(sizeof(RayGenUniforms));
            rayHitUniformBuffer = Buffer::UniformBuffer(sizeof(RayHitUniforms), bounces + 1);

        }

        void PathTracingRenderer::Render(Ref<RenderTarget> renderTarget, Ref<Scene::Scene> scene,
            ivec2 imageSubdivisions, Graphics::CommandList* commandList) {

            if (!scene->IsRtDataValid())
                return;

            Graphics::Profiler::BeginQuery("Path tracing");

            auto& camera = scene->GetMainCamera();
            auto width = renderTarget->GetScaledWidth();
            auto height = renderTarget->GetScaledHeight();

            auto rayCount = realTime ? 2 * width * height * realTimeSamplesPerFrame : 2 * width * height;

            if (glm::distance(camera.GetLocation(), cameraLocation) > 1e-3f ||
                glm::distance(camera.rotation, cameraRotation) > 1e-3f ||
                helper.GetRayBuffer()->GetElementCount() != rayCount) {
                cameraLocation = camera.GetLocation();
                cameraRotation = camera.rotation;

                sampleCount = 0;
                imageOffset = ivec2(0);
                // Helper automatically double buffers, no need for 2 * rayCount
                helper.SetRayBufferSize(rayCount / 2);
            }

            if (realTime) {
                imageSubdivisions = ivec2(1);
                sampleCount = 0;
                frameCount++;
            }

            rayGenPipelineConfig.ManageMacro("REALTIME", realTime);
            rayHitPipelineConfig.ManageMacro("REALTIME", realTime);

            helper.UpdateLights(scene, sampleEmissives);

            ivec2 resolution = ivec2(width, height);
            ivec2 tileSize = resolution / imageSubdivisions;

            // The number of bounces may change
            if (rayHitUniformBuffer.GetElementCount() != bounces + 1) {
                rayHitUniformBuffer.SetSize(bounces + 1);
            }

            auto samplesPerFrame = realTime ? realTimeSamplesPerFrame : 1;
#ifdef AE_OS_MACOS
            // MoltenVK and Metal don't seem to support atomic add, so limit to 1 sample per frame
            samplesPerFrame = 1;
#endif

            for (int32_t i = 0; i <= bounces; i++) {
                RayHitUniforms uniforms;
                uniforms.maxBounces = bounces;

                uniforms.sampleCount = sampleCount;
                uniforms.bounceCount = i;

                uniforms.resolution = resolution;
                uniforms.seed = Common::Random::SampleFastUniformFloat();

                uniforms.exposure = camera.exposure;

                uniforms.samplesPerFrame = samplesPerFrame;
                uniforms.maxRadiance = maxRadiance;

                uniforms.frameCount = frameCount;

                rayHitUniformBuffer.SetData(&uniforms, i);
            }            

            imageBarriers.clear();
            imageBarriers.push_back({ renderTarget->outputTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
            imageBarriers.push_back({ renderTarget->radianceTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
            imageBarriers.push_back({ renderTarget->historyRadianceTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT });

            auto rtData = renderTarget->GetData(FULL_RES);
            auto velocityTexture = rtData->velocityTexture;
            auto depthTexture = rtData->depthTexture;
            auto normalTexture = rtData->normalTexture;
            auto materialIdxTexture = rtData->materialIdxTexture;
            auto baseColorTexture = rtData->baseColorTexture;

            auto historyRtData = renderTarget->GetHistoryData(FULL_RES);
            auto historyDepthTexture = historyRtData->depthTexture;
            auto historyNormalTexture = historyRtData->normalTexture;
            auto historyMaterialIdxTexture = historyRtData->materialIdxTexture;

            if (!realTime) {
                commandList->BindImage(renderTarget->outputTexture.image, 3, 1);
                commandList->BindImage(renderTarget->radianceTexture.image, 3, 3);
                commandList->BindImage(renderTarget->historyRadianceTexture.image, 3, 2);
            }
            else {
                imageBarriers.push_back({ renderTarget->lightingTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->frameAccumTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->radianceTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ velocityTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ depthTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ normalTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ materialIdxTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ baseColorTexture->image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });

                commandList->BindImage(renderTarget->lightingTexture.image, 3, 0);
                commandList->BindImage(renderTarget->frameAccumTexture.image, 3, 1);
                commandList->BindImage(renderTarget->radianceTexture.image, 3, 3);

                renderTarget->historyRadianceTexture.Bind(commandList, 3, 2);

                commandList->BindImage(velocityTexture->image, 3, 5);
                commandList->BindImage(depthTexture->image, 3, 6);
                commandList->BindImage(normalTexture->image, 3, 7);
                commandList->BindImage(materialIdxTexture->image, 3, 8);
                commandList->BindImage(baseColorTexture->image, 3, 9);
            }

            commandList->PipelineBarrier(imageBarriers, {});

            auto tileResolution = resolution / imageSubdivisions;
            auto groupCount = tileResolution / 8;

            groupCount.x += ((groupCount.x * 8 == tileResolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == tileResolution.y) ? 0 : 1);

            Graphics::Profiler::BeginQuery("Ray generation");

            helper.DispatchRayGen(scene, commandList, PipelineManager::GetPipeline(rayGenPipelineConfig),
                ivec3(groupCount.x, groupCount.y, samplesPerFrame), false,
                [=]() {
                    auto corners = camera.GetFrustumCorners(camera.nearPlane, camera.farPlane);

                    RayGenUniforms uniforms;
                    uniforms.origin = vec4(corners[4], 1.0f);
                    uniforms.right = vec4(corners[5] - corners[4], 1.0f);
                    uniforms.bottom = vec4(corners[6] - corners[4], 1.0f);

                    uniforms.sampleCount = realTime ? frameCount : sampleCount;
                    uniforms.pixelOffset = ivec2(renderTarget->GetWidth(),
                        renderTarget->GetHeight()) / imageSubdivisions * imageOffset;

                    uniforms.tileSize = tileSize;
                    uniforms.resolution = resolution;

                    rayGenUniformBuffer.SetData(&uniforms, 0);
                    commandList->BindBuffer(rayGenUniformBuffer.Get(), 3, 4);
                }
                );

            
            for (int32_t i = 0; i <= bounces; i++) {

                commandList->ImageMemoryBarrier(renderTarget->frameAccumTexture.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                commandList->ImageMemoryBarrier(velocityTexture->image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                Graphics::Profiler::EndAndBeginQuery("Bounce " + std::to_string(i));

                helper.DispatchHitClosest(scene, commandList, PipelineManager::GetPipeline(rayHitPipelineConfig), false, true,
                    [=]() {
                        commandList->BindBufferOffset(rayHitUniformBuffer.Get(),
                            rayHitUniformBuffer.GetAlignedOffset(i), 3, 4);
                    }
                    );
            }

            Graphics::Profiler::EndQuery();

            if (realTime) {

                imageBarriers.clear();

                imageBarriers.push_back({ renderTarget->frameAccumTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ velocityTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ depthTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ normalTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ materialIdxTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ baseColorTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ historyDepthTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ historyNormalTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ historyMaterialIdxTexture->image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });

                commandList->PipelineBarrier(imageBarriers, {});

                velocityTexture->Bind(commandList, 3, 4);
                depthTexture->Bind(commandList, 3, 5);
                normalTexture->Bind(commandList, 3, 6);
                materialIdxTexture->Bind(commandList, 3, 7);
                historyDepthTexture->Bind(commandList, 3, 9);
                historyNormalTexture->Bind(commandList, 3, 10);
                historyMaterialIdxTexture->Bind(commandList, 3, 11);

                struct alignas(16) PushConstants {
                    float historyClipMax;
                    float currentClipFactor;
                    float maxHistoryLength;
                    float exposure;
                    int samplesPerFrame;
                    float maxRadiance;
                };

                Graphics::Profiler::BeginQuery("Temporal");

                PushConstants constants = {
                    .historyClipMax = historyClipMax,
                    .currentClipFactor = currentClipFactor,
                    .maxHistoryLength = float(historyLengthMax),
                    .exposure = camera.exposure,
                    .samplesPerFrame = samplesPerFrame,
                    .maxRadiance = maxRadiance
                };
                commandList->PushConstants("constants", &constants);

                auto pipelineConfig = PipelineConfig("pathtracer/temporal.csh");
                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);

                groupCount = resolution / 16;

                groupCount.x += ((groupCount.x * 16 == resolution.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == resolution.y) ? 0 : 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();

            }

            imageOffset.x++;

            if (imageOffset.x == imageSubdivisions.x) {
                imageOffset.x = 0;
                imageOffset.y++;
            }

            if (imageOffset.y == imageSubdivisions.y) {
                imageOffset.y = 0;
                sampleCount++;
            }

            commandList->ImageTransition(renderTarget->outputTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            commandList->ImageTransition(renderTarget->lightingTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            commandList->ImageTransition(renderTarget->radianceTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            helper.InvalidateRayBuffer(commandList);

            Graphics::Profiler::EndQuery();

        }

        void PathTracingRenderer::ResetSampleCount() {

            sampleCount = 0;

        }

        int32_t PathTracingRenderer::GetSampleCount() const {

            return sampleCount;

        }

    }

}