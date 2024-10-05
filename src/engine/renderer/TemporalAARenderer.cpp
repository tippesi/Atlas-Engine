#include "TemporalAARenderer.h"
#include "helper/GeometryHelper.h"

#include "Clock.h"

namespace Atlas {

    namespace Renderer {

        void TemporalAARenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            pipelineConfig = PipelineConfig("taa.csh");
            PipelineManager::AddPipeline(pipelineConfig);

        }

        void TemporalAARenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("TAA");

            pipelineConfig.ManageMacro("PATHTRACE", false);

            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            auto res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

            const int32_t groupSize = 8;
            ivec2 groupCount = res / groupSize;
            groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            auto& camera = scene->GetMainCamera();

            auto targetData = target->GetData(FULL_RES);

            const auto lastHistory = target->GetLastHistory();
            const auto history = target->GetHistory();
            const auto lastVelocity = target->GetLastVelocity();
            auto const velocity = target->GetVelocity();
            const auto depth = targetData->depthTexture;
            const auto stencil = targetData->stencilTexture;

            Graphics::ImageBarrier imageBarriers[] = {
                {lastHistory->image,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT},
                {history->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT},
                {lastVelocity->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT}
            };
            commandList->PipelineBarrier(imageBarriers, {});

            commandList->BindImage(lastHistory->image, 3, 0);

            commandList->BindImage(history->image, history->sampler, 3, 1);
            commandList->BindImage(target->lightingTexture.image, target->lightingTexture.sampler, 3, 2);
            commandList->BindImage(velocity->image, velocity->sampler, 3, 3);
            commandList->BindImage(depth->image, depth->sampler, 3, 4);
            commandList->BindImage(lastVelocity->image, lastVelocity->sampler, 3, 5);
            commandList->BindImage(stencil->image, stencil->sampler, 3, 6);

            auto constants = PushConstants {
                .resolution = vec2((float)target->GetScaledWidth(), (float)target->GetScaledHeight()),
                .invResolution = 1.0f / vec2((float)target->GetScaledWidth(), (float)target->GetScaledHeight()),
                .jitter = camera.GetJitter(),
                .resetHistory = !target->HasHistory() ? 1 : 0
            };

            commandList->PushConstants("constants", &constants);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(lastHistory->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            Graphics::Profiler::EndQuery();

        }

    }

}