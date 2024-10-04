#include "RayTracingHelper.h"
#include "../../common/ColorConverter.h"
#include "../../common/RandomHelper.h"
#include "../../common/Piecewise.h"
#include "../../volume/BVH.h"
#include "../../graphics/Profiler.h"
#include "../../pipeline/PipelineManager.h"
#include <glm/gtx/norm.hpp>

#define DIRECTIONAL_LIGHT 0
#define TRIANGLE_LIGHT 1
#define POINT_LIGHT 2
#define SPOT_LIGHT 3

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            RayTracingHelper::RayTracingHelper() {

                const size_t lightCount = 128;

                indirectDispatchBuffer = Buffer::Buffer(Buffer::BufferUsageBits::IndirectBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit,
                    3 * sizeof(uint32_t), 0);
                counterBuffer0 = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit,
                    sizeof(uint32_t), 0);
                counterBuffer1 = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit,
                    sizeof(uint32_t), 0);

                indirectDispatchBuffer.SetSize(1);
                counterBuffer0.SetSize(1);
                counterBuffer1.SetSize(1);

                // Create dynamic resizable shader storage buffers
                lightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit |
                    Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit,
                    sizeof(GPULight), lightCount);
                rayBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit, 3 * sizeof(vec4));
                rayPayloadBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit, sizeof(vec4));
                rayBinCounterBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit, sizeof(uint32_t));
                rayBinOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HighPriorityMemoryBit, sizeof(uint32_t));

                rayBinCounterBuffer.SetSize(1024);
                rayBinOffsetBuffer.SetSize(1024);

                traceDispatchPipelineConfig = PipelineConfig("raytracer/traceDispatch.csh");
                traceClosestPipelineConfig = PipelineConfig("raytracer/traceClosest.csh");
                traceAnyPipelineConfig = PipelineConfig("raytracer/traceAny.csh");

                binningOffsetPipelineConfig = PipelineConfig("raytracer/binning.csh");
                binningPipelineConfig = PipelineConfig("raytracer/binningOffset.csh");

                std::vector<uint8_t> dummyData = { 0 };
                dummyTexture = Texture::Texture2DArray(1, 1, 1, VK_FORMAT_R8_UNORM);
                dummyTexture.SetData(dummyData, 0);

            }

            void RayTracingHelper::SetRayBufferSize(size_t size) {

                // We use double buffering
                rayBuffer.SetSize(size * 2);
                rayPayloadBuffer.SetSize(size * 2);

            }

            void RayTracingHelper::DispatchAndHit(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                const Ref<Graphics::Pipeline>& dispatchAndHitPipeline,
                glm::ivec3 dimensions, std::function<void(void)> prepare) {

                if (!scene->IsRtDataValid()) return;

                // Select lights once per initial ray dispatch
                {
                    auto lightCount = lightBuffer.GetElementCount();
                    selectedLights.clear();
                    // Randomly select lights (only at image offset 0)
                    if (lights.size() > lightCount && stochasticLightSelection) {
                        std::vector<float> weights;
                        weights.reserve(lights.size());
                        for (auto& light : lights) {
                            weights.push_back(light.data.y);
                        }

                        auto piecewiseDistribution = Common::Piecewise1D(weights);

                        for (size_t i = 0; i < lightCount; i++) {                            
                            float pdf = 0.0f;
                            int32_t offset = 0;
                            piecewiseDistribution.Sample(pdf, offset);

                            selectedLights.push_back(lights[offset]);
                        }

                        for (auto& light : selectedLights) {
                            light.data.y *= float(selectedLights.size());
                        }
                    }
                    else if (lights.size() > lightCount && !stochasticLightSelection) {
                        auto& camera = scene->GetMainCamera();
                        auto cameraLocation = camera.GetLocation();

                        std::sort(lights.begin(), lights.end(), [&](const GPULight& light0, const GPULight& light1) {  
                            return glm::distance2(vec3(light0.P), cameraLocation)
                               < glm::distance2(vec3(light1.P), cameraLocation);
                        });

                        for (size_t i = 0; i < lightCount; i++) {
                            auto& light = lights[i];

                            light.data.y = 1.0f;
                            selectedLights.push_back(light);
                        }
                    }
                    else {
                        for (auto& light : lights) {
                            light.data.y = 1.0f;
                            selectedLights.push_back(light);
                        }
                    }

                    lightBuffer.SetData(selectedLights.data(), 0, selectedLights.size());
                }

                // Bind textures and buffers
                {
                    const auto& rtData = scene->rayTracingWorld;

                    if (scene->sky.GetProbe())
                        commandList->BindImage(scene->sky.GetProbe()->GetCubemap().image,
                            scene->sky.GetProbe()->GetCubemap().sampler, 2, 6);
                    else
                        commandList->BindImage(dummyTexture.image, dummyTexture.sampler, 2, 6);

                    rtData->materialBuffer.Bind(commandList, 2, 7);
                    rtData->bvhInstanceBuffer.Bind(commandList, 2, 21);
                    if (rtData->includeObjectHistory)
                        rtData->lastMatricesBuffer.Bind(commandList, 2, 27);
                    lightBuffer.Bind(commandList, 2, 11);

                    if (rtData->hardwareRayTracing) {
                        commandList->BindTLAS(rtData->tlas, 2, 23);
                    }
                    else {
                        rtData->tlasNodeBuffer.Bind(commandList, 2, 22);
                    }
                }

                // Execute shader
                {
                    commandList->BindPipeline(dispatchAndHitPipeline);

                    PushConstants constants;
                    constants.lightCount = int32_t(selectedLights.size());
                    commandList->PushConstants("constants", &constants);

                    prepare();

                    commandList->Dispatch(dimensions.x, dimensions.y, dimensions.z);
                }

            }

            void RayTracingHelper::DispatchRayGen(Ref<Scene::Scene> scene, 
                Graphics::CommandList* commandList, const Ref<Graphics::Pipeline>& rayGenPipeline, 
                glm::ivec3 dimensions, bool binning, std::function<void(void)> prepare) {

                if (!scene->IsRtDataValid()) return;

                dispatchCounter = 0;
                rayOffsetCounter = 0;
                payloadOffsetCounter = 0;

                // Select lights once per initial ray dispatch
                {
                    auto lightCount = lightBuffer.GetElementCount();
                    selectedLights.clear();
                    // Randomly select lights (only at image offset 0)
                    if (lights.size() > lightCount && stochasticLightSelection) {
                        std::vector<float> weights;
                        weights.reserve(lights.size());
                        for (auto& light : lights) {
                            weights.push_back(light.data.y);
                        }

                        auto piecewiseDistribution = Common::Piecewise1D(weights);

                        for (size_t i = 0; i < lightCount; i++) {
                            float pdf = 0.0f;
                            int32_t offset = 0;
                            piecewiseDistribution.Sample(pdf, offset);

                            selectedLights.push_back(lights[offset]);
                        }

                        for (auto& light : selectedLights) {
                            light.data.y *= float(selectedLights.size());
                        }
                    }
                    else if (lights.size() > lightCount && !stochasticLightSelection) {
                        auto& camera = scene->GetMainCamera();
                        auto cameraLocation = camera.GetLocation();

                        std::sort(lights.begin(), lights.end(), [&](const GPULight& light0, const GPULight& light1) {
                            return glm::distance2(vec3(light0.P), cameraLocation)
                                < glm::distance2(vec3(light1.P), cameraLocation);
                            });

                        for (size_t i = 0; i < lightCount; i++) {
                            auto& light = lights[i];

                            light.data.y = 1.0f;
                            selectedLights.push_back(light);
                        }
                    }
                    else {
                        for (auto& light : lights) {
                            light.data.y = 1.0f;
                            selectedLights.push_back(light);
                        }
                    }

                    lightBuffer.SetData(selectedLights.data(), 0, selectedLights.size());
                }

                // From last invalidation we need to take care of the transfer stage as well
                commandList->BufferMemoryBarrier(counterBuffer0.Get(), VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                commandList->BufferMemoryBarrier(counterBuffer1.Get(), VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                commandList->BufferMemoryBarrier(rayBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);
                commandList->BufferMemoryBarrier(rayPayloadBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);

                commandList->BindBuffer(counterBuffer0.Get(), 2, 14);
                commandList->BindBuffer(counterBuffer1.Get(), 2, 13);

                commandList->BindBuffer(rayBuffer.Get(), 2, 15);
                commandList->BindBuffer(rayPayloadBuffer.Get(), 2, 16);

                commandList->BindBuffer(rayBinCounterBuffer.Get(), 2, 17);

                commandList->BindPipeline(rayGenPipeline);

                PushConstants constants;
                constants.lightCount = int32_t(selectedLights.size());
                constants.rayBufferOffset = uint32_t(rayOffsetCounter++);
                constants.rayBufferSize = uint32_t(rayBuffer.GetElementCount() / 2);
                constants.useRayBinning = binning ? 1 : 0;

                commandList->PushConstants("constants", &constants);

                prepare();
                commandList->Dispatch(dimensions.x, dimensions.y, dimensions.z);
            }

            void RayTracingHelper::DispatchHitClosest(Ref<Scene::Scene> scene, 
                Graphics::CommandList* commandList, const Ref<Graphics::Pipeline>& hitPipeline, 
                bool binning, bool opacityCheck, std::function<void(void)> prepare) {

                if (!scene->IsRtDataValid()) return;

                // Bind textures and buffers
                {
                    const auto& rtData = scene->rayTracingWorld;

                    if (scene->sky.GetProbe())
                        commandList->BindImage(scene->sky.GetProbe()->GetCubemap().image,
                            scene->sky.GetProbe()->GetCubemap().sampler, 2, 6);
                    else
                        commandList->BindImage(dummyTexture.image, dummyTexture.sampler, 2, 6);

                    rtData->materialBuffer.Bind(commandList, 2, 7);
                    rtData->bvhInstanceBuffer.Bind(commandList, 2, 21);
                    if (rtData->includeObjectHistory)
                        rtData->lastMatricesBuffer.Bind(commandList, 2, 27);
                    lightBuffer.Bind(commandList, 2, 11);

                    if (rtData->hardwareRayTracing) {
                        commandList->BindTLAS(rtData->tlas, 2, 23);
                    }
                    else {
                        rtData->tlasNodeBuffer.Bind(commandList, 2, 22);
                    }
                }

                Graphics::Profiler::BeginQuery("Setup command buffer");

                commandList->BindBuffer(indirectDispatchBuffer.Get(), 2, 12);

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers;

                // Set up command buffer, reset ray count
                {
                    auto pipeline = PipelineManager::GetPipeline(traceDispatchPipelineConfig);
                    commandList->BindPipeline(pipeline);

                    // Can't group barriers together because of different access patterns
                    commandList->BufferMemoryBarrier(indirectDispatchBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

                    if (dispatchCounter % 2 == 0) {
                        bufferBarriers.push_back({ counterBuffer0.Get(), VK_ACCESS_SHADER_READ_BIT });
                        bufferBarriers.push_back({ counterBuffer1.Get(), VK_ACCESS_SHADER_WRITE_BIT });
                        commandList->BindBuffer(counterBuffer0.Get(), 2, 13);
                        commandList->BindBuffer(counterBuffer1.Get(), 2, 14);
                    }
                    else {
                        bufferBarriers.push_back({ counterBuffer0.Get(), VK_ACCESS_SHADER_WRITE_BIT });
                        bufferBarriers.push_back({ counterBuffer1.Get(), VK_ACCESS_SHADER_READ_BIT });
                        commandList->BindBuffer(counterBuffer0.Get(), 2, 14);
                        commandList->BindBuffer(counterBuffer1.Get(), 2, 13);
                    }
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                    commandList->Dispatch(1, 1, 1);
                }

                bufferBarriers.clear();
                imageBarriers.clear();

                // Can't group barriers together because of different access patterns
                commandList->BufferMemoryBarrier(indirectDispatchBuffer.Get(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

                bufferBarriers.push_back({ rayBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                bufferBarriers.push_back({ rayPayloadBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                commandList->BindBuffer(rayBuffer.Get(), 2, 15);
                commandList->BindBuffer(rayPayloadBuffer.Get(), 2, 16);

                commandList->BindBuffer(rayBinCounterBuffer.Get(), 2, 17);
                commandList->BindBuffer(rayBinOffsetBuffer.Get(), 2, 18);

                /*
                if (binning) {

                    Graphics::Profiler::EndAndBeginQuery("Binning offsets");

                    // Calculate binning offsets
                    {
                        auto pipeline = PipelineManager::GetPipeline(binningOffsetPipelineConfig);
                        commandList->BindPipeline(pipeline);

                        commandList->BindBuffer(rayBinCounterBuffer.Get(), 2, 15);
                        commandList->BindBuffer(rayBinOffsetBuffer.Get(), 2, 16);


                        // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                        // glDispatchCompute(1, 1, 1);
                    }

                    Graphics::Profiler::EndAndBeginQuery("Binning rays");

                    // Order rays by their bins
                    {
                        auto pipeline = PipelineManager::GetPipeline(binningPipelineConfig);
                        commandList->BindPipeline(pipeline);


                        binningShader.GetUniform("rayBufferOffset")->SetValue(uint32_t(rayOffsetCounter++ % 2));
                        binningShader.GetUniform("rayPayloadBufferOffset")->SetValue(uint32_t(payloadOffsetCounter++ % 2));
                        binningShader.GetUniform("rayBufferSize")->SetValue(uint32_t(rayBuffer.GetElementCount() / 2));

                        // glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
                        // glDispatchComputeIndirect(0);

                        uint32_t zero = 0;
                        rayBinCounterBuffer.Bind();
                        rayBinCounterBuffer.InvalidateData();
                        // rayBinCounterBuffer.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);
                    }

                }
                */

                Graphics::Profiler::EndAndBeginQuery("Trace rays");

                // Trace rays for closest intersection
                {
                    auto pipelineConfig = traceClosestPipelineConfig;
                    if (opacityCheck) {
                        pipelineConfig.AddMacro("OPACITY_CHECK");
                    }
                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                    commandList->BindPipeline(pipeline);

                    PushConstants constants;
                    constants.rayBufferOffset = uint32_t(rayOffsetCounter++ % 2);
                    constants.rayBufferSize = uint32_t(rayBuffer.GetElementCount() / 2);

                    commandList->PushConstants("constants", &constants);

                    commandList->DispatchIndirect(indirectDispatchBuffer.Get());
                }

                Graphics::Profiler::EndAndBeginQuery("Execute hit shader");

                bufferBarriers.clear();
                imageBarriers.clear();

                bufferBarriers.push_back({ rayBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                bufferBarriers.push_back({ rayPayloadBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });

                if (dispatchCounter % 2 == 0) {
                    bufferBarriers.push_back({ counterBuffer1.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                }
                else {
                    bufferBarriers.push_back({ counterBuffer0.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                }
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);
                
                // Shade rays
                {
                    commandList->BindPipeline(hitPipeline);

                    PushConstants constants;
                    constants.lightCount = int32_t(selectedLights.size());
                    constants.rayBufferOffset = uint32_t(rayOffsetCounter++ % 2);
                    constants.rayPayloadBufferOffset = uint32_t(payloadOffsetCounter++ % 2);
                    constants.rayBufferSize = uint32_t(rayBuffer.GetElementCount() / 2);
                    constants.useRayBinning = binning ? 1 : 0;

                    commandList->PushConstants("constants", &constants);
                    //rayBinCounterBuffer.BindBase(11);

                    prepare();

                    commandList->DispatchIndirect(indirectDispatchBuffer.Get());
                }

                Graphics::Profiler::EndQuery();

                dispatchCounter++;

            }

            void RayTracingHelper::InvalidateRayBuffer(Graphics::CommandList* commandList) {

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers;

                bufferBarriers.push_back({counterBuffer0.Get(), VK_ACCESS_TRANSFER_WRITE_BIT});
                bufferBarriers.push_back({counterBuffer1.Get(), VK_ACCESS_TRANSFER_WRITE_BIT});
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

                uint32_t zero = 0;
                commandList->FillBuffer(counterBuffer0.Get(), &zero);
                commandList->FillBuffer(counterBuffer1.Get(), &zero);

                bufferBarriers.clear();
                bufferBarriers.push_back({counterBuffer0.Get(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT});
                bufferBarriers.push_back({counterBuffer1.Get(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT});
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            }

            Buffer::Buffer* RayTracingHelper::GetRayBuffer() {

                // This is the latest write buffer in all tracing functions
                return &rayBuffer;

            }

            void RayTracingHelper::UpdateLights(Ref<Scene::Scene> scene, bool useEmissivesAsLights) {

                lights.clear();

                auto& camera = scene->GetMainCamera();
                auto cameraLocation = camera.GetLocation();

                auto lightSubset = scene->GetSubset<LightComponent>();
                for (auto& lightEntity : lightSubset) {
                    auto& light = lightEntity.GetComponent<LightComponent>();

                    auto radiance = Common::ColorConverter::ConvertSRGBToLinear(light.color) * light.intensity;
                    auto brightness = dot(radiance, vec3(0.3333f));

                    vec3 P = vec3(0.0f);
                    vec3 N = vec3(0.0f);
                    float weight = 0.0f;
                    float radius = 1.0f;
                    float specific0 = 0.0f;
                    float specific1 = 0.0f;

                    uint32_t data0 = 0, data1 = 0;

                    const auto& prop = light.transformedProperties;
                    // Parse individual light information based on type
                    if (light.type == LightType::DirectionalLight) {
                        data0 |= (DIRECTIONAL_LIGHT << 28u);
                        weight = brightness;
                        P = cameraLocation;
                        N = light.transformedProperties.directional.direction;
                    }
                    else if (light.type == LightType::PointLight) {
                        data0 |= (POINT_LIGHT << 28u);
                        weight = brightness;
                        P = light.transformedProperties.point.position;
                        radius = light.transformedProperties.point.radius;
                    }
                    else if (light.type == LightType::SpotLight) {
                        data0 |= (SPOT_LIGHT << 28u);
                        weight = brightness;
                        P = light.transformedProperties.spot.position;
                        N = light.transformedProperties.spot.direction;
                        radius = light.transformedProperties.spot.radius;

                        auto cosOuter = cosf(prop.spot.outerConeAngle);
                        auto cosInner = cosf(prop.spot.innerConeAngle);
                        auto lightAngleScale = 1.0f / std::max(0.001f, cosInner - cosOuter);
                        auto lightAngleOffset = -cosOuter * lightAngleScale;

                        specific0 = lightAngleScale;
                        specific1 = lightAngleOffset;
                    }

                    if (light.shadow) {
                        data1 |= 1u;
                    }

                    data0 |= uint32_t(lights.size());
                    auto castData0 = reinterpret_cast<float&>(data0);
                    auto castData1 = reinterpret_cast<float&>(data1);

                    GPULight gpuLight;
                    gpuLight.P = vec4(P, castData1);
                    gpuLight.N = vec4(N, radius);
                    gpuLight.color = vec4(radiance, 0.0f);
                    gpuLight.data = vec4(castData0, weight, specific0, specific1);

                    lights.push_back(gpuLight);
                }

                if (useEmissivesAsLights && scene->IsRtDataValid()) {
                    const auto& rtData = scene->rayTracingWorld;
                    lights.insert(lights.end(), rtData->triangleLights.begin(), rtData->triangleLights.end());
                }
                
                if (stochasticLightSelection) {
                    // Find the maximum weight
                    auto maxWeight = 0.0f;
                    for (auto& light : lights) {
                        maxWeight = glm::max(maxWeight, light.data.y);
                    }

                    // Calculate min weight and adjust lights based on it
                    auto minWeight = 0.005f * maxWeight;
                    // Also calculate the total weight
                    auto totalWeight = 0.0f;

                    for (auto& light : lights) {
                        light.data.y = glm::max(light.data.y, minWeight);
                        totalWeight += light.data.y;
                    }

                    for (auto& light : lights) {
                        light.data.y /= totalWeight;
                    }
                }
            }
            

        }

    }

}