#include "RTAORenderer.h"

namespace Atlas {

	namespace Renderer {

		RTAORenderer::RTAORenderer() {

            const int32_t filterSize = 21;
			blurFilter.CalculateGaussianFilter(float(filterSize) / 6.0f, filterSize);

            rtaoShader.AddStage(AE_COMPUTE_STAGE, "ao/rtao.csh");
            rtaoShader.Compile();

			bilateralBlurShader.AddStage(AE_VERTEX_STAGE, "bilateralBlur.vsh");
			bilateralBlurShader.AddStage(AE_FRAGMENT_STAGE, "bilateralBlur.fsh");

			bilateralBlurShader.AddMacro("BILATERAL");
			bilateralBlurShader.AddMacro("BLUR_R");

			bilateralBlurShader.Compile();

		}

		void RTAORenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            auto ssao = scene->ssao;
            if (!ssao || !ssao->enable) return;

            helper.SetScene(scene, 8);

            ivec2 res = ivec2(target->ssaoTexture.width, target->ssaoTexture.height);

            // Calculate RTAO
            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                helper.DispatchAndHit(&rtaoShader, ivec3(groupCount, 1), 
                    [=]() {
                        target->ssaoTexture.Bind(GL_WRITE_ONLY, 3);

                        // Bind the geometry normal texure and depth texture
                        target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE0);
                        target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

                        ssao->noiseTexture.Bind(GL_TEXTURE2);

                        rtaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                        rtaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                        rtaoShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

                        rtaoShader.GetUniform("sampleCount")->SetValue(ssao->sampleCount);
                        rtaoShader.GetUniform("radius")->SetValue(ssao->radius);
                        rtaoShader.GetUniform("resolution")->SetValue(res);

                    });
            }

            framebuffer.Bind();

            // Blur AO
            {
                bilateralBlurShader.Bind();

                target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets);

                auto mean = (kernelWeights.size() - 1) / 2;
                kernelWeights = std::vector<float>(kernelWeights.begin() + mean, kernelWeights.end());
                kernelOffsets = std::vector<float>(kernelOffsets.begin() + mean, kernelOffsets.end());

                bilateralBlurShader.GetUniform("weight")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                bilateralBlurShader.GetUniform("offset")->SetValue(kernelOffsets.data(), (int32_t)kernelOffsets.size());

                bilateralBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size());

                glViewport(0, 0, res.x, res.y);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &target->swapSsaoTexture);
                target->ssaoTexture.Bind(GL_TEXTURE0);

                bilateralBlurShader.GetUniform("blurDirection")->SetValue(
                    vec2(1.0f / float(res.x), 0.0f)
                );

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &target->ssaoTexture);
                target->swapSsaoTexture.Bind(GL_TEXTURE0);

                bilateralBlurShader.GetUniform("blurDirection")->SetValue(
                    vec2(0.0f, 1.0f / float(res.y))
                );

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }            

		}

	}

}