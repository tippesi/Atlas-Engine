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

		void TemporalAARenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

			Graphics::Profiler::BeginQuery("TAA");

            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

			auto res = ivec2(target->GetWidth(), target->GetHeight());

			const int32_t groupSize = 8;
			ivec2 groupCount = res / groupSize;
			groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
			groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            auto targetData = target->GetData(FULL_RES);

            auto lastHistory = target->GetLastHistory();
            auto history = target->GetHistory();
            auto lastVelocity = target->GetLastVelocity();
            auto velocity = target->GetVelocity();
            auto depth = targetData->depthTexture;
            auto stencil = targetData->stencilTexture;

            std::vector<Graphics::BufferBarrier> bufferBarriers;
            std::vector<Graphics::ImageBarrier> imageBarriers;
            imageBarriers.push_back({lastHistory->image,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({history->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({lastVelocity->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT});
            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

            commandList->BindImage(lastHistory->image, 3, 0);

            commandList->BindImage(history->image, history->sampler, 3, 1);
            commandList->BindImage(target->lightingTexture.image, target->lightingTexture.sampler, 3, 2);
            commandList->BindImage(velocity->image, velocity->sampler, 3, 3);
            commandList->BindImage(depth->image, depth->sampler, 3, 4);
            commandList->BindImage(lastVelocity->image, lastVelocity->sampler, 3, 5);
            commandList->BindImage(stencil->image, stencil->sampler, 3, 6);

            auto constants = PushConstants {
                .resolution = vec2((float)target->GetWidth(), (float)target->GetHeight()),
                .invResolution = 1.0f / vec2((float)target->GetWidth(), (float)target->GetHeight()),
                .jitter = camera->GetJitter()
            };
            commandList->PushConstants("constants", &constants);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

			Graphics::Profiler::EndQuery();

		}

	}

}