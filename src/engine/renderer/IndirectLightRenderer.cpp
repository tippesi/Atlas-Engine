#include "IndirectLightRenderer.h"

namespace Atlas {

    namespace Renderer {

        void IndirectLightRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            diffusePipelineConfig = PipelineConfig("deferred/indirectDiffuse.csh");
            specularPipelineConfig = PipelineConfig("deferred/indirectSpecular.csh");

            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));

        }

        void IndirectLightRenderer::RenderIndirectDiffuse(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Indirect diffuse lighting");

            auto volume = scene->irradianceVolume;
            auto reflection = scene->reflection;
            auto ssgi = scene->ssgi;
            auto rtgi = scene->rtgi;

            auto rtDataValid = scene->IsRtDataValid();
            auto rtgiEnabled = rtgi && rtgi->enable && rtDataValid;
            auto ddgiEnabled = volume && volume->enable && !rtgiEnabled && rtDataValid;
            auto ddgiVisibility = volume && volume->enable && rtDataValid && volume->visibility;
            auto reflectionEnabled = reflection && reflection->enable && (rtDataValid || reflection->ssr);
            auto ssgiEnabled = ssgi && ssgi->enable && !rtgiEnabled;
            bool ssgiAo = ssgiEnabled && ssgi->enableAo;            

            diffusePipelineConfig.ManageMacro("RTGI", rtgiEnabled);
            diffusePipelineConfig.ManageMacro("DDGI", ddgiEnabled);
            diffusePipelineConfig.ManageMacro("DDGI_SCROLL", ddgiEnabled && volume->scroll);
            diffusePipelineConfig.ManageMacro("VISIBILITY_VOLUME", ddgiVisibility);
            diffusePipelineConfig.ManageMacro("REFLECTION", reflectionEnabled);
            diffusePipelineConfig.ManageMacro("SSGI", ssgiEnabled);

            auto depthTexture = target->GetData(HALF_RES)->depthTexture;
            auto normalTexture = target->GetData(HALF_RES)->normalTexture;

            auto pipeline = PipelineManager::GetPipeline(diffusePipelineConfig);
            commandList->BindPipeline(pipeline);

            if (ssgiEnabled || rtgiEnabled) {
                commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 1);
            }
            auto uniforms = Uniforms {
                .reflectionDownsampled2x = target->GetReflectionResolution() == RenderResolution::HALF_RES,
                .giDownsampled2x = target->GetGIResolution() == RenderResolution::HALF_RES,
                .aoStrength =  ssgiAo ? ssgi->aoStrength / sqrt(ssgi->radius) : 1.0f,
                .specularProbeMipLevels = int32_t(scene->sky.GetProbe() ? scene->sky.GetProbe()->GetCubemap().image->mipLevels : 1)
            };
            uniformBuffer.SetData(&uniforms, 0);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 3);
            commandList->BindBuffer(uniformBuffer.Get(), 3, 4);

            auto resolution = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            auto groupCount = resolution / 8;

            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();

        }

        void IndirectLightRenderer::RenderIndirectSpecular(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Indirect specular lighting");

            auto volume = scene->irradianceVolume;
            auto reflection = scene->reflection;
            auto ssgi = scene->ssgi;
            auto rtgi = scene->rtgi;

            auto rtDataValid = scene->IsRtDataValid();
            auto rtgiEnabled = rtgi && rtgi->enable && rtDataValid;
            auto ddgiEnabled = volume && volume->enable && !rtgiEnabled && rtDataValid;
            auto ddgiVisibility = volume && volume->enable && rtDataValid && volume->visibility;
            auto reflectionEnabled = reflection && reflection->enable && (rtDataValid || reflection->ssr);
            auto ssgiEnabled = ssgi && ssgi->enable && !rtgiEnabled;
            bool ssgiAo = ssgiEnabled && ssgi->enableAo;

            specularPipelineConfig.ManageMacro("RTGI", rtgiEnabled);
            specularPipelineConfig.ManageMacro("DDGI", ddgiEnabled);
            specularPipelineConfig.ManageMacro("DDGI_SCROLL", ddgiEnabled && volume->scroll);
            specularPipelineConfig.ManageMacro("VISIBILITY_VOLUME", ddgiVisibility);
            specularPipelineConfig.ManageMacro("REFLECTION", reflectionEnabled);
            specularPipelineConfig.ManageMacro("SSGI", ssgiEnabled);

            auto depthTexture = target->GetData(HALF_RES)->depthTexture;
            auto normalTexture = target->GetData(HALF_RES)->normalTexture;

            auto pipeline = PipelineManager::GetPipeline(specularPipelineConfig);
            commandList->BindPipeline(pipeline);

            if (reflectionEnabled) {
                commandList->BindImage(target->reflectionTexture.image, target->reflectionTexture.sampler, 3, 1);
            }
            if (ssgiEnabled || rtgiEnabled) {
                commandList->BindImage(target->giTexture.image, target->giTexture.sampler, 3, 2);
            }

            auto uniforms = Uniforms{ 
                .reflectionDownsampled2x = target->GetReflectionResolution() == RenderResolution::HALF_RES,
                .giDownsampled2x = target->GetGIResolution() == RenderResolution::HALF_RES,
                .aoStrength =  ssgiAo ? ssgi->aoStrength / sqrt(ssgi->radius) : 1.0f,
                .specularProbeMipLevels = int32_t(scene->sky.GetProbe() ? scene->sky.GetProbe()->GetCubemap().image->mipLevels : 1)
            };
            uniformBuffer.SetData(&uniforms, 0);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 3);
            commandList->BindImage(normalTexture->image, normalTexture->sampler, 3, 4);
            commandList->BindBuffer(uniformBuffer.Get(), 3, 5);

            auto resolution = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            auto groupCount = resolution / 8;

            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();

        }

    }

}
