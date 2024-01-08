#include "IndirectLightRenderer.h"

namespace Atlas {

    namespace Renderer {

        void IndirectLightRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            pipelineConfig = PipelineConfig("deferred/indirect.csh");
            PipelineManager::AddPipeline(pipelineConfig);

            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));

        }

        void IndirectLightRenderer::Render(Viewport* viewport, RenderTarget* target,
            Camera* camera, Scene::Scene* scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Indirect lighting");

            auto volume = scene->irradianceVolume;
            auto ao = scene->ao;
            auto reflection = scene->reflection;
            auto ssgi = scene->ssgi;

            auto rtDataValid = scene->IsRtDataValid();
            auto ddgiEnabled = volume && volume->enable && rtDataValid;
            auto reflectionEnabled = reflection && reflection->enable && rtDataValid;
            auto aoEnabled = ao && ao->enable && (!ao->rt || rtDataValid);
            auto ssgiEnabled = ssgi && ssgi->enable && (!ssgi->rt || rtDataValid);
            bool ssgiAo = ssgi && ssgi->enable && ssgi->enableAo;            

            pipelineConfig.ManageMacro("DDGI", ddgiEnabled);
            pipelineConfig.ManageMacro("REFLECTION", reflectionEnabled);
            pipelineConfig.ManageMacro("AO", aoEnabled);
            pipelineConfig.ManageMacro("SSGI", ssgiEnabled);

            auto depthTexture = target->GetData(HALF_RES)->depthTexture;

            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            if (aoEnabled) {
                commandList->BindImage(target->aoTexture.image, target->aoTexture.sampler, 3, 1);
            }
            if (reflectionEnabled) {
                commandList->BindImage(target->reflectionTexture.image, target->reflectionTexture.sampler, 3, 2);
            }
            if (ssgiEnabled) {
                commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 3);
            }

            auto uniforms = Uniforms{
                .aoEnabled = aoEnabled || ssgiAo ? 1 : 0,
                .aoDownsampled2x = ssgiAo ? target->GetGIResolution() == RenderResolution::HALF_RES : 
                    target->GetAOResolution() == RenderResolution::HALF_RES,
                .reflectionEnabled = reflectionEnabled ? 1 : 0,
                .aoStrength = aoEnabled || ssgiAo ? (aoEnabled ? ao->strength : ssgi->aoStrength / sqrt(ssgi->radius)) : 1.0f,
                .specularProbeMipLevels = int32_t(scene->sky.GetProbe() ? scene->sky.GetProbe()->cubemap.image->mipLevels : 1)
            };
            uniformBuffer.SetData(&uniforms, 0);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);
            commandList->BindBuffer(uniformBuffer.Get(), 3, 5);

            auto resolution = ivec2(target->GetWidth(), target->GetHeight());
            auto groupCount = resolution / 8;

            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();

        }

    }

}
