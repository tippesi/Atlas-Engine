#include "DDGIRenderer.h"

#include "../common/RandomHelper.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void DDGIRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);
            Helper::GeometryHelper::GenerateSphereVertexArray(sphereArray, 10, 10);

            rayHitBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4));
            rayGenUniformBuffer = Buffer::UniformBuffer(sizeof(RayGenUniforms));
            rayHitUniformBuffer = Buffer::UniformBuffer(sizeof(RayHitUniforms));

            rayGenPipelineConfig = PipelineConfig("ddgi/rayGen.csh");
            rayHitPipelineConfig = PipelineConfig("ddgi/rayHit.csh");

            probeStatePipelineConfig = PipelineConfig("ddgi/probeState.csh");
            probeIrradianceUpdatePipelineConfig = PipelineConfig("ddgi/probeUpdate.csh", {"IRRADIANCE"});
            probeMomentsUpdatePipelineConfig = PipelineConfig("ddgi/probeUpdate.csh");
            irradianceCopyEdgePipelineConfig = PipelineConfig("ddgi/copyEdge.csh", {"IRRADIANCE"});
            momentsCopyEdgePipelineConfig = PipelineConfig("ddgi/copyEdge.csh");

            probeDebugMaterial = CreateRef<Material>();
            probeDebugActiveMaterial = CreateRef<Material>();
            probeDebugInactiveMaterial = CreateRef<Material>();
            probeDebugOffsetMaterial = CreateRef<Material>();

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void DDGIRenderer::TraceAndUpdateProbes(Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto volume = scene->irradianceVolume;
            if (!volume || !volume->enable || !volume->update || !scene->IsRtDataValid())
                return;

            Graphics::Profiler::BeginQuery("DDGI");

            Ref<Lighting::Shadow> shadow = nullptr;
            auto mainLightEntity = GetMainLightEntity(scene);
            if (mainLightEntity.IsValid())
                shadow = mainLightEntity.GetComponent<LightComponent>().shadow;

            rayHitPipelineConfig.ManageMacro("USE_SHADOW_MAP", shadow && volume->useShadowMap);
            
            auto& internalVolume = volume->internal;
            internalVolume.SwapTextures();

            auto totalProbeCount = volume->probeCount.x *
                volume->probeCount.y *
                volume->probeCount.z * 
                volume->cascadeCount;

            auto rayCount = volume->rayCount;
            auto rayCountInactive = volume->rayCountInactive;

            auto totalRayCount = rayCount * totalProbeCount;

            Graphics::Profiler::BeginQuery("Scene update");

            if (totalRayCount != rayHitBuffer.GetElementCount()) {
                helper.SetRayBufferSize(totalRayCount);
                rayHitBuffer.SetSize(totalRayCount);
            }

            auto [irradianceArray, momentsArray] = internalVolume.GetCurrentProbes();
            auto [lastIrradianceArray, lastMomentsArray] = internalVolume.GetLastProbes();

            auto& rayDirBuffer = internalVolume.rayDirBuffer;
            auto& rayDirInactiveBuffer = internalVolume.rayDirInactiveBuffer;
            auto& probeStateBuffer = internalVolume.probeStateBuffer;
            auto& probeOffsetBuffer = internalVolume.probeOffsetBuffer;

            helper.UpdateLights(scene, volume->sampleEmissives);

            commandList->BindBuffer(probeStateBuffer.Get(), 2, 19);
            commandList->BindBuffer(probeOffsetBuffer.Get(), 2, 20);

            auto probeCount = volume->probeCount * ivec3(1, volume->cascadeCount, 1);

            Graphics::Profiler::EndAndBeginQuery("Ray generation");

            auto rayGenPipeline = PipelineManager::GetPipeline(rayGenPipelineConfig);
            helper.DispatchRayGen(scene, commandList, rayGenPipeline, probeCount, false,
                [&]() {
                    using namespace Common;

                    auto theta = acosf(2.0f * float(rand()) / float(RAND_MAX) - 1.0f);
                    auto phi = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);

                    auto dir = glm::vec3(
                        2.0f * Random::SampleUniformFloat() - 1.0f,
                        2.0f * Random::SampleUniformFloat() - 1.0f,
                        2.0f * Random::SampleUniformFloat() - 1.0f
                    );

                    auto epsilon = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);
                    auto rot = mat3(glm::rotate(epsilon, dir));

                    auto uniforms = RayGenUniforms {
                        .rotationMatrix = mat4(rot)
                    };
                    rayGenUniformBuffer.SetData(&uniforms, 0);

                    commandList->BindBuffer(rayDirBuffer.Get(), 3, 0);
                    commandList->BindBuffer(rayDirInactiveBuffer.Get(), 3, 1);
                    commandList->BindBuffer(rayGenUniformBuffer.Get(), 3, 2);
                }
            );

            Graphics::Profiler::EndAndBeginQuery("Ray evaluation");

            commandList->BindImage(lastIrradianceArray.image, lastIrradianceArray.sampler, 2, 24);
            commandList->BindImage(lastMomentsArray.image, lastMomentsArray.sampler, 2, 25);

            auto rayHitPipeline = PipelineManager::GetPipeline(rayHitPipelineConfig);
            helper.DispatchHitClosest(scene, commandList, rayHitPipeline, false, volume->opacityCheck,
                [&]() {
                    RayHitUniforms uniforms;
                    uniforms.seed = Common::Random::SampleUniformFloat();

                    if (shadow && volume->useShadowMap) {
                        auto &shadowUniform = uniforms.shadow;
                        shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                        shadowUniform.bias = shadow->bias;
                        shadowUniform.edgeSoftness = shadow->edgeSoftness;
                        shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                        shadowUniform.cascadeCount = shadow->viewCount;
                        shadowUniform.resolution = vec2(shadow->resolution);

                        commandList->BindImage(shadow->maps.image, shadowSampler, 3, 0);

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
                            } else {
                                auto cascade = &shadow->views[componentCount - 1];
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                            }
                        }
                    }
                    rayHitUniformBuffer.SetData(&uniforms, 0);

                    // Use this buffer instead of the default writeRays buffer of the helper
                    commandList->BindBuffer(rayHitBuffer.Get(), 3, 1);
                    commandList->BindBuffer(rayHitUniformBuffer.Get(), 3, 2);
                }
            );

            commandList->BufferMemoryBarrier(rayHitBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            commandList->BindBuffer(rayHitBuffer.Get(), 3, 1);

            commandList->ImageMemoryBarrier(irradianceArray.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            commandList->ImageMemoryBarrier(momentsArray.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            Graphics::Profiler::EndAndBeginQuery("Update probes");

            // Update the probes
            {
                Graphics::Profiler::BeginQuery("Update irradiance");

                commandList->BindImage(irradianceArray.image, 3, 0);

                auto pipeline = PipelineManager::GetPipeline(probeIrradianceUpdatePipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->Dispatch(probeCount.x, probeCount.y, probeCount.z);

                Graphics::Profiler::EndAndBeginQuery("Update moments");

                commandList->BindImage(momentsArray.image, 3, 0);

                probeMomentsUpdatePipelineConfig.ManageMacro("LOWER_RES_MOMENTS", volume->lowerResMoments);
                pipeline = PipelineManager::GetPipeline(probeMomentsUpdatePipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->Dispatch(probeCount.x, probeCount.y, probeCount.z);

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::EndAndBeginQuery("Update probe states");

            // Update the states of the probes
            {
                commandList->BufferMemoryBarrier(probeStateBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                auto pipeline = PipelineManager::GetPipeline(probeStatePipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->Dispatch(probeCount.x, probeCount.y, probeCount.z);
            }

            Graphics::Profiler::EndAndBeginQuery("Update probe edges");

            // Copy the probe edges
            {
                commandList->ImageMemoryBarrier(irradianceArray.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                auto pipeline = PipelineManager::GetPipeline(irradianceCopyEdgePipelineConfig);
                commandList->BindPipeline(pipeline);

                auto probeRes = volume->irrRes;

                auto res = ivec2(irradianceArray.width, irradianceArray.height);
                auto groupCount = res / 8;

                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                commandList->PushConstants("constants", &probeRes, sizeof(int32_t));
                commandList->BindImage(irradianceArray.image, 3, 0);
                commandList->Dispatch(groupCount.x, groupCount.y, probeCount.y);

                commandList->ImageMemoryBarrier(momentsArray.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                pipeline = PipelineManager::GetPipeline(momentsCopyEdgePipelineConfig);
                commandList->BindPipeline(pipeline);

                probeRes = volume->momRes;

                res = ivec2(momentsArray.width, momentsArray.height);
                groupCount = res / 8;

                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                commandList->PushConstants("constants", &probeRes, sizeof(int32_t));
                commandList->BindImage(momentsArray.image, 3, 0);
                commandList->Dispatch(groupCount.x, groupCount.y, probeCount.y);

            }

            helper.InvalidateRayBuffer(commandList);

            VkPipelineStageFlags destinationShaderStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            if (volume->debug)
                destinationShaderStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            commandList->BufferMemoryBarrier(probeStateBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            commandList->BufferMemoryBarrier(probeOffsetBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            commandList->ImageMemoryBarrier(irradianceArray.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, destinationShaderStage);
            commandList->ImageMemoryBarrier(momentsArray.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, destinationShaderStage);

            commandList->BindImage(irradianceArray.image, irradianceArray.sampler, 2, 24);
            commandList->BindImage(momentsArray.image, momentsArray.sampler, 2, 25);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

        void DDGIRenderer::DebugProbes(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
            std::unordered_map<void*, uint16_t>& materialMap) {

            auto volume = scene->irradianceVolume;
            if (!volume || !volume->enable || !volume->update || !volume->debug)
                return;

            // Need additional barrier, since in the normal case DDGI is made to be sampled just in compute shader
            auto& internalVolume = volume->internal;
            auto [irradianceArray, momentsArray] = internalVolume.GetCurrentProbes();

            // Need to rebind after barrier
            commandList->BindImage(irradianceArray.image, irradianceArray.sampler, 2, 24);
            commandList->BindImage(momentsArray.image, momentsArray.sampler, 2, 25);

            auto shaderConfig = ShaderConfig {
                {"ddgi/probeDebug.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"ddgi/probeDebug.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .frameBuffer = target->gBufferFrameBuffer,
                .vertexInputInfo = sphereArray.GetVertexInputState(),
                .rasterizer = Graphics::Initializers::InitPipelineRasterizationStateCreateInfo(
                    VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE)
            };
            auto pipelineConfig = PipelineConfig(shaderConfig, pipelineDesc);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            probeDebugActiveMaterial->emissiveColor = vec3(0.0f, 1.0f, 0.0f);
            probeDebugInactiveMaterial->emissiveColor = vec3(1.0f, 0.0f, 0.0f);
            probeDebugOffsetMaterial->emissiveColor = vec3(0.0f, 0.0f, 1.0f);

            sphereArray.Bind(commandList);

            ProbeDebugConstants constants = {
                .probeMaterialIdx = uint32_t(materialMap[probeDebugMaterial.get()]),
                .probeActiveMaterialIdx = uint32_t(materialMap[probeDebugActiveMaterial.get()]),
                .probeInactiveMaterialIdx = uint32_t(materialMap[probeDebugInactiveMaterial.get()]),
                .probeOffsetMaterialIdx = uint32_t(materialMap[probeDebugOffsetMaterial.get()])
            };
            commandList->PushConstants("constants", &constants);

            auto instanceCount = volume->probeCount.x * volume->probeCount.y * volume->probeCount.z * volume->cascadeCount;
            commandList->DrawIndexed(sphereArray.GetIndexComponent().elementCount, uint32_t(instanceCount));

        }

    }

}