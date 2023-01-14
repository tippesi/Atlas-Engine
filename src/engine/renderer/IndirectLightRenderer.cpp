#include "IndirectLightRenderer.h"

namespace Atlas {

	namespace Renderer {

        void IndirectLightRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            pipelineConfig = PipelineConfig("deferred/indirect.csh");
            PipelineManager::AddPipeline(pipelineConfig);

            uniformBuffer = Buffer::Buffer(Buffer::BufferUsageBits::UniformBuffer |
                Buffer::BufferUsageBits::MultiBuffered | Buffer::BufferUsageBits::HostAccess,
                sizeof(Uniforms), 1);

        }

		void IndirectLightRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene, Graphics::CommandList* commandList) {

			Graphics::Profiler::BeginQuery("Indirect lighting");

            auto volume = scene->irradianceVolume;
            auto ao = scene->ao;
            auto reflection = scene->reflection;

            pipelineConfig.ManageMacro("DDGI", volume && volume->enable);
            pipelineConfig.ManageMacro("REFLECTION", reflection && reflection->enable);
            pipelineConfig.ManageMacro("AO", ao && ao->enable);

            auto depthTexture = target->GetData(HALF_RES)->depthTexture;

            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
			commandList->BindPipeline(pipeline);

            if (ao && ao->enable) {
                commandList->BindImage(target->aoTexture.image, target->aoTexture.sampler, 3, 1);
            }
            if (reflection && reflection->enable) {
                commandList->BindImage(target->reflectionTexture.image, target->reflectionTexture.sampler, 3, 2);
            }

            auto uniforms = Uniforms {
                .aoEnabled = ao && ao->enable ? 1 : 0,
                .aoDownsampled2x = target->GetAOResolution() == RenderResolution::HALF_RES,
                .reflectionEnabled = reflection && reflection->enable ? 1 : 0,
                .aoStrength = ao && ao->enable ? ao->strength : 1.0f
            };
            uniformBuffer.SetData(&uniforms, 0, 1);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 3);
            commandList->BindBuffer(uniformBuffer.GetMultiBuffer(), 3, 4);

			auto resolution = ivec2(target->GetWidth(), target->GetHeight());
			auto groupCount = resolution / 8;

			groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
			groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

			commandList->Dispatch(groupCount.x, groupCount.y, 1);

			Graphics::Profiler::EndQuery();

		}

	}

}
