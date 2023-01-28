#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

        void SkyboxRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            pipelineConfig = PipelineConfig("skybox.csh");

        }

		void SkyboxRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

			Graphics::Profiler::BeginQuery("Skybox");

            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rtData = target->GetData(FULL_RES);
            auto velocityTexture = rtData->velocityTexture;
            auto depthTexture = rtData->depthTexture;

            std::vector<Graphics::BufferBarrier> bufferBarriers;
            std::vector<Graphics::ImageBarrier> imageBarriers = {
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {velocityTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

            vec4 lastCameraLocation = vec4(camera->GetLastLocation(), 1.0f);
            auto constantRange = pipeline->shader->GetPushConstantRange("constants");
            commandList->PushConstants(constantRange, &lastCameraLocation);

            const auto& cubeMap = scene->sky.probe->cubemap;

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(velocityTexture->image, 3, 1);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindImage(cubeMap.image, cubeMap.sampler, 3, 3);

            auto resolution = ivec2(target->GetWidth(), target->GetHeight());
            auto groupCount = resolution / 8;

            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            imageBarriers = {
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {velocityTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            Graphics::Profiler::EndQuery();

		}

	}

}