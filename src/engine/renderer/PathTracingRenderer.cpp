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

            rayGenPipelineConfig = PipelineConfig("pathtracer/rayGen.csh");
            rayHitPipelineConfig = PipelineConfig("pathtracer/rayHit.csh");

            rayGenUniformBuffer = Buffer::UniformBuffer(sizeof(RayGenUniforms));
            rayHitUniformBuffer = Buffer::UniformBuffer(sizeof(RayHitUniforms), bounces + 1);

        }

        void PathTracingRenderer::Render(Ref<PathTracerRenderTarget> renderTarget, Ref<Scene::Scene> scene,
            ivec2 imageSubdivisions, Graphics::CommandList* commandList) {

            if (!scene->IsRtDataValid())
                return;

            Graphics::Profiler::BeginQuery("Path tracing");

            auto& camera = scene->GetMainCamera();
            auto width = renderTarget->GetWidth();
            auto height = renderTarget->GetHeight();

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
                renderTarget->Swap();
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

            std::vector<Graphics::ImageBarrier> imageBarriers;
            std::vector<Graphics::BufferBarrier> bufferBarriers;

            imageBarriers.push_back({ renderTarget->texture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
            imageBarriers.push_back({ renderTarget->radianceTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
            imageBarriers.push_back({ renderTarget->historyRadianceTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT });

            if (!realTime) {
                commandList->BindImage(renderTarget->texture.image, 3, 1);
                commandList->BindImage(renderTarget->radianceTexture.image, 3, 3);
                commandList->BindImage(renderTarget->historyRadianceTexture.image, 3, 2);
            }
            else {
                imageBarriers.push_back({ renderTarget->frameAccumTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->radianceTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->velocityTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->depthTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->normalTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->materialIdxTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });
                imageBarriers.push_back({ renderTarget->albedoTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT });

                commandList->BindImage(renderTarget->frameAccumTexture.image, 3, 1);
                commandList->BindImage(renderTarget->radianceTexture.image, 3, 3);

                renderTarget->historyRadianceTexture.Bind(commandList, 3, 2);

                commandList->BindImage(renderTarget->velocityTexture.image, 3, 5);
                commandList->BindImage(renderTarget->depthTexture.image, 3, 6);
                commandList->BindImage(renderTarget->normalTexture.image, 3, 7);
                commandList->BindImage(renderTarget->materialIdxTexture.image, 3, 8);
                commandList->BindImage(renderTarget->albedoTexture.image, 3, 9);
            }

            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

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
                commandList->ImageMemoryBarrier(renderTarget->velocityTexture.image, VK_IMAGE_LAYOUT_GENERAL,
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
                imageBarriers.push_back({ renderTarget->velocityTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->depthTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->normalTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->materialIdxTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->albedoTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->historyDepthTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->historyNormalTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });
                imageBarriers.push_back({ renderTarget->historyMaterialIdxTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT });

                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                commandList->BindImage(renderTarget->texture.image, 3, 0);

                renderTarget->velocityTexture.Bind(commandList, 3, 4);
                renderTarget->depthTexture.Bind(commandList, 3, 5);
                renderTarget->normalTexture.Bind(commandList, 3, 6);
                renderTarget->materialIdxTexture.Bind(commandList, 3, 7);
                renderTarget->historyDepthTexture.Bind(commandList, 3, 9);
                renderTarget->historyNormalTexture.Bind(commandList, 3, 10);
                renderTarget->historyMaterialIdxTexture.Bind(commandList, 3, 11);

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

                // We don't want to swap already here when rendering in realtime
                if (!realTime)
                    renderTarget->Swap();
            }

            commandList->ImageTransition(renderTarget->texture.image,
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