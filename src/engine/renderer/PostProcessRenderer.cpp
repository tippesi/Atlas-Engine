#include "PostProcessRenderer.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

        void PostProcessRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            auto mainShaderConfig = ShaderConfig {
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto mainPipelineDesc = Graphics::GraphicsPipelineDesc();
            mainPipelineSwapChainConfig = PipelineConfig(mainShaderConfig, mainPipelineDesc);
            mainPipelineFrameBufferConfig = PipelineConfig(mainShaderConfig, mainPipelineDesc);

            sharpenPipelineConfig = PipelineConfig("sharpen.csh");

            Graphics::BufferDesc bufferDesc {
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = sizeof(Uniforms)
            };
            uniformBuffer = device->CreateMultiBuffer(bufferDesc);

        }

		void PostProcessRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Postprocessing");

            auto& postProcessing = scene->postProcessing;

            auto& chromaticAberration = postProcessing.chromaticAberration;
            auto& vignette = postProcessing.vignette;
            auto& taa = postProcessing.taa;
            auto& sharpen = postProcessing.sharpen;

            ivec2 resolution = ivec2(target->GetWidth(), target->GetHeight());

            if (sharpen.enable) {
                Graphics::Profiler::BeginQuery("Sharpen");

                auto pipeline = PipelineManager::GetPipeline(sharpenPipelineConfig);

                commandList->BindPipeline(pipeline);

                ivec2 groupCount = resolution / 8;
                groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

                auto& image = target->hdrTexture.image;

                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                commandList->BindImage(image, 3, 0);

                if (taa.enable) {
                    commandList->ImageMemoryBarrier(target->GetHistory()->image,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                    commandList->BindImage(target->GetHistory()->image,
                        target->GetHistory()->sampler, 3, 1);
                }
                else {
                    commandList->BindImage(target->lightingTexture.image,
                        target->lightingTexture.sampler, 3, 1);
                }


                /*
                 * if (taa.enable) {
                    target->GetHistory()->Bind(0);
                }
                else {
                    target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(0);
                }
                 *
                 */

                auto constantRange = pipeline->shader->GetPushConstantRange("constants");
                commandList->PushConstants(constantRange, &sharpen.factor);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                Graphics::Profiler::EndQuery();
            }

            {
                Graphics::Profiler::BeginQuery("Main");

                commandList->BeginRenderPass(device->swapChain, true);

                auto pipelineConfig = GetMainPipelineConfig();
                pipelineConfig.ManageMacro("FILMIC_TONEMAPPING", postProcessing.filmicTonemapping);
                pipelineConfig.ManageMacro("VIGNETTE", postProcessing.vignette.enable);
                pipelineConfig.ManageMacro("CHROMATIC_ABERRATION", postProcessing.chromaticAberration.enable);

                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);

                SetUniforms(camera, scene);

                commandList->BindImage(target->hdrTexture.image, target->hdrTexture.sampler, 3, 0);
                commandList->BindBuffer(uniformBuffer, 3, 4);

                commandList->Draw(6, 1, 0, 0);

                commandList->EndRenderPass();

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::EndQuery();

		}

		void PostProcessRenderer::SetUniforms(Camera* camera, Scene::Scene* scene) {

            auto& postProcessing = scene->postProcessing;

            auto& chromaticAberration = postProcessing.chromaticAberration;
            auto& vignette = postProcessing.vignette;

            Uniforms uniforms = {
                .exposure = camera->exposure,
                .saturation = postProcessing.saturation,
                .timeInMilliseconds = Clock::Get() * 1000.0f,
            };

            if (chromaticAberration.enable) {
                uniforms.aberrationStrength = chromaticAberration.strength;
                uniforms.aberrationReversed = chromaticAberration.colorsReversed ? 1.0f : 0.0f;
            }
            else {
                uniforms.aberrationStrength = 0.0f;
            }

            if (vignette.enable) {
                uniforms.vignetteOffset = vignette.offset;
                uniforms.vignettePower = vignette.power;
                uniforms.vignetteStrength = vignette.strength;
                uniforms.vignetteColor = vec4(vignette.color, 0.0f);
            }

            uniformBuffer->SetData(&uniforms, 0, sizeof(Uniforms));

		}

        PipelineConfig PostProcessRenderer::GetMainPipelineConfig() {

            auto shaderConfig = ShaderConfig {
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .swapChain = device->swapChain
            };
            return PipelineConfig(shaderConfig, pipelineDesc);

        }

        PipelineConfig PostProcessRenderer::GetMainPipelineConfig(const Ref<Graphics::FrameBuffer> frameBuffer) {

            auto shaderConfig = ShaderConfig {
                { "postprocessing.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "postprocessing.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = frameBuffer
            };
            return PipelineConfig(shaderConfig, pipelineDesc);

        }

	}

}