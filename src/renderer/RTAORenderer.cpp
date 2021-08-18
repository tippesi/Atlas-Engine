#include "RTAORenderer.h"

namespace Atlas {

	namespace Renderer {

		RTAORenderer::RTAORenderer() {

            const int32_t filterSize = 21;
			blurFilter.CalculateGaussianFilter(float(filterSize) / 6.0f, filterSize);

            rtaoShader.AddStage(AE_COMPUTE_STAGE, "rtao.csh");
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

            // Calculate RTAO
            {
                ivec2 groupCount = ivec2(ssao->map.width / 8, ssao->map.height / 4);
                groupCount.x += ((groupCount.x * 8 == ssao->map.width) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == ssao->map.height) ? 0 : 1);

                helper.DispatchAndHit(&rtaoShader, ivec3(groupCount, 1), 
                    [=]() {
                        ssao->map.Bind(GL_WRITE_ONLY, 3);

                        // Bind the geometry normal texure and depth texture
                        target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE0);
                        target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

                        ssao->noiseTexture.Bind(GL_TEXTURE2);

                        rtaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                        rtaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                        rtaoShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

                        rtaoShader.GetUniform("sampleCount")->SetValue(ssao->sampleCount);
                        rtaoShader.GetUniform("radius")->SetValue(ssao->radius);
                        rtaoShader.GetUniform("resolution")->SetValue(ivec2(ssao->map.width, ssao->map.height));


                    });
            }

            framebuffer.Bind();

            // Blur SSAO
            {
                bilateralBlurShader.Bind();

                target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets);

                bilateralBlurShader.GetUniform("weight")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                bilateralBlurShader.GetUniform("offset")->SetValue(kernelOffsets.data(), (int32_t)kernelOffsets.size());

                bilateralBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size());

                glViewport(0, 0, ssao->map.width, ssao->map.height);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &ssao->blurMap);
                ssao->map.Bind(GL_TEXTURE0);

                bilateralBlurShader.GetUniform("blurDirection")->SetValue(
                    vec2(1.0f / float(ssao->map.width), 0.0f)
                );

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &ssao->map);
                ssao->blurMap.Bind(GL_TEXTURE0);

                bilateralBlurShader.GetUniform("blurDirection")->SetValue(
                    vec2(0.0f, 1.0f / float(ssao->map.height))
                );

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }            

		}

	}

}