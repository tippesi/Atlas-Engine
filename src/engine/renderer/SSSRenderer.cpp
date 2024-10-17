#include "SSSRenderer.h"

namespace Atlas {

    namespace Renderer {

        void SSSRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            pipelineConfig = PipelineConfig("deferred/sss.csh");

        }

        void SSSRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto sss = scene->sss;

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!sss || !sss->enable || !mainLightEntity.IsValid()) return;

            Graphics::Profiler::BeginQuery("Screen space shadows");

            auto& camera = scene->GetMainCamera();

            ivec2 res = ivec2(target->sssTexture.width, target->sssTexture.height);
            auto downsampledTarget = target->GetData(RenderResolution::FULL_RES);

            auto depthTexture = downsampledTarget->depthTexture;
            auto normalTexture = downsampledTarget->geometryNormalTexture;

            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                static int frameCount = 0;

                auto& light = mainLightEntity.GetComponent<LightComponent>();
                auto lightDirection = glm::normalize(vec3(camera.viewMatrix * vec4(
                    light.transformedProperties.directional.direction, 0.0f)));

                pipelineConfig.ManageMacro("TRACE_WORLD_SPACE", sss->traceWorldSpace);

                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->ImageMemoryBarrier(target->sssTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                commandList->BindImage(target->sssTexture.image, 3, 0);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 1);
                commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 2);

                PushConstants constants = {
                    .lightDirection = vec4(lightDirection, 0.0),
                    .sampleCount = sss->sampleCount,
                    .maxLength = sss->maxLength,
                    .minLengthWorldSpace = sss->minLengthWorldSpace,
                    .thickness = sss->thickness
                };
                commandList->PushConstants("constants", &constants);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(target->sssTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }

            // Possible blur pass to get rid of noise
            {

            }

            Graphics::Profiler::EndQuery();

        }

    }

}