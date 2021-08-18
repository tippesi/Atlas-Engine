#include "SSAORenderer.h"

namespace Atlas {

	namespace Renderer {

        SSAORenderer::SSAORenderer() {

            blurFilter.CalculateBoxFilter(11);

            ssaoShader.AddStage(AE_VERTEX_STAGE, "deferred/ssao.vsh");
            ssaoShader.AddStage(AE_FRAGMENT_STAGE, "deferred/ssao.fsh");

            ssaoShader.Compile();

            bilateralBlurShader.AddStage(AE_VERTEX_STAGE, "bilateralBlur.vsh");
            bilateralBlurShader.AddStage(AE_FRAGMENT_STAGE, "bilateralBlur.fsh");

            bilateralBlurShader.AddMacro("BILATERAL");
            bilateralBlurShader.AddMacro("BLUR_R");

            bilateralBlurShader.Compile();

        }

        void SSAORenderer::Render(Viewport* viewport, RenderTarget* target,
            Camera* camera, Scene::Scene* scene) {

            auto ssao = scene->ssao;
            if (!ssao || !ssao->enable) return;

            framebuffer.Bind();

            // Calculate SSAO
            {
                glViewport(0, 0, ssao->map.width, ssao->map.height);
                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &ssao->map);

                ssaoShader.Bind();

                ssaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                ssaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);

                ssaoShader.GetUniform("sampleCount")->SetValue(ssao->sampleCount);
                ssaoShader.GetUniform("samples")->SetValue(ssao->samples.data(), int32_t(ssao->samples.size()));
                ssaoShader.GetUniform("radius")->SetValue(ssao->radius);
                ssaoShader.GetUniform("resolution")->SetValue(vec2(float(ssao->map.width), float(ssao->map.height)));

                // Bind the geometry normal texure and depth texture
                target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE0);
                target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);
                
                ssao->noiseTexture.Bind(GL_TEXTURE2);

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }

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