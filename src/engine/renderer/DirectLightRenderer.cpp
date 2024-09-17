#include "DirectLightRenderer.h"

namespace Atlas {

    namespace Renderer {

        void DirectLightRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            lightCullingBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));
            cloudShadowUniformBuffer = Buffer::UniformBuffer(sizeof(CloudShadow));

            pipelineConfig = PipelineConfig("deferred/direct.csh");

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void DirectLightRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, 
            Graphics::CommandList* commandList) {

            auto renderState = &scene->renderState;
            if (renderState->lightEntities.empty()) return;

            Graphics::Profiler::BeginQuery("Direct lighting");

            auto mainLightEntity = GetMainLightEntity(scene);
            auto& camera = scene->GetMainCamera();
            auto& light = mainLightEntity.GetComponent<LightComponent>();
            auto sss = scene->sss;
            auto clouds = scene->sky.clouds;

            ivec2 res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            int32_t groupSize = 16;
            ivec2 groupCount = res / groupSize;
            groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            if (lightCullingBuffer.GetElementCount() < groupCount.x * groupCount.y * 128) {
                lightCullingBuffer.SetSize(groupCount.x * groupCount.y * 128);
            }

            Graphics::Profiler::BeginQuery("Light culling");

            commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);

            CullingPushConstants cullingPushConstants;
#ifdef AE_BINDLESS
            cullingPushConstants.lightCount = std::min(4096, int32_t(renderState->lightEntities.size()));
#else
            cullingPushConstants.lightCount = std::min(8, int32_t(renderState->lightEntities.size()));
#endif

            lightCullingBuffer.Bind(commandList, 3, 6);

            auto cullingPipelineConfig = PipelineConfig("deferred/lightCulling.csh");
            auto pipeline = PipelineManager::GetPipeline(cullingPipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->PushConstants("constants", &cullingPushConstants);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndAndBeginQuery("Lighting");

            PushConstants pushConstants;
#ifdef AE_BINDLESS
            pushConstants.lightCount = std::min(4096, int32_t(renderState->lightEntities.size()));
            pushConstants.lightBucketCount = 1;
#else
            std::vector<Ref<Graphics::Image>> cascadeMaps;
            std::vector<Ref<Graphics::Image>> cubeMaps;

            pushConstants.lightCount = std::min(8, int32_t(renderState->lightEntities.size()));
            for (int32_t i = 0; i < pushConstants.lightCount; i++) {
                auto& comp = renderState->lightEntities[i].comp;

                if (comp.shadow) {
                    auto& shadow = comp.shadow;
                    if (shadow->useCubemap) {
                        pushConstants.mapIndices[i] = int32_t(cubeMaps.size());
                        cubeMaps.push_back(shadow->cubemap->image);
                    }
                    else {
                        pushConstants.mapIndices[i] = int32_t(cascadeMaps.size());
                        cascadeMaps.push_back(shadow->maps->image);
                    }
                }
            }

            commandList->BindSampledImages(cascadeMaps, 3, 7);
            commandList->BindSampledImages(cubeMaps, 3, 15);
#endif

            pipelineConfig.ManageMacro("SCREEN_SPACE_SHADOWS", sss && sss->enable);
            pipelineConfig.ManageMacro("CLOUD_SHADOWS", clouds && clouds->enable && clouds->castShadow);
            pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->BindImage(target->lightingTexture.image, 3, 0);

            if (sss && sss->enable) {
                commandList->BindImage(target->sssTexture.image, target->sssTexture.sampler, 3, 1);
            }

            commandList->BindSampler(shadowSampler, 3, 4);

            CloudShadow cloudShadowUniform;
            if (clouds && clouds->enable && clouds->castShadow) {
                clouds->shadowTexture.Bind(commandList, 3, 2);

                clouds->GetShadowMatrices(camera, glm::normalize(light.transformedProperties.directional.direction),
                    cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);

                cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera.invViewMatrix;
            }

            cloudShadowUniformBuffer.SetData(&cloudShadowUniform, 0);
            cloudShadowUniformBuffer.Bind(commandList, 3, 5);            

            commandList->PushConstants("constants", &pushConstants);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

    }

}
