#include "DirectLightRenderer.h"

namespace Atlas {

    namespace Renderer {

        void DirectLightRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

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
            Helper::LightData& lightData, Graphics::CommandList* commandList) {

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!mainLightEntity.IsValid()) return;

            Graphics::Profiler::BeginQuery("Direct lighting");

            auto& camera = scene->GetMainCamera();
            auto& light = mainLightEntity.GetComponent<LightComponent>();
            auto sss = scene->sss;
            auto clouds = scene->sky.clouds;

            std::vector<Ref<Graphics::Image>> cascadeMaps;
            std::vector<Ref<Graphics::Image>> cubeMaps;

            PushConstants pushConstants;
            pushConstants.lightCount = std::min(8, int32_t(lightData.lightEntities.size()));
            for (int32_t i = 0; i < pushConstants.lightCount; i++) {
                auto& comp = lightData.lightEntities[i].comp;

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

            commandList->PushConstants("constants", &pushConstants);

            commandList->BindSampledImages(cascadeMaps, 3, 6);
            commandList->BindSampledImages(cubeMaps, 3, 14);

            pipelineConfig.ManageMacro("SCREEN_SPACE_SHADOWS", sss && sss->enable);
            pipelineConfig.ManageMacro("CLOUD_SHADOWS", clouds && clouds->enable && clouds->castShadow);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->BindImage(target->lightingTexture.image, 3, 0);

            if (sss && sss->enable) {
                commandList->BindImage(target->sssTexture.image, target->sssTexture.sampler, 3, 1);
            }

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
            cloudShadowUniformBuffer.Bind(commandList, 3, 4);

            commandList->BindSampler(shadowSampler, 3, 5);

            ivec2 res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            int32_t groupSize = 8;
            ivec2 groupCount = res / groupSize;
            groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();

        }

    }

}
