#include "BlurRenderer.h"

namespace Atlas {

    namespace Renderer {

        BlurRenderer::BlurRenderer(std::string vertexSource, std::string fragmentSource, int32_t channelCount,
                float* kernelOffsets, float* kernelWeights, int32_t kernelSize, bool bilateral) : bilateralBlur(bilateral) {

            shader.AddStage(AE_VERTEX_STAGE, vertexSource);
            shader.AddStage(AE_FRAGMENT_STAGE, fragmentSource);

            if (bilateral) {
                shader.AddMacro("BILATERAL");
            }

            if (channelCount == 1) {
                shader.AddMacro("BLUR_R");
            }
            else if (channelCount == 2) {
                shader.AddMacro("BLUR_RG");
            }
            else if (channelCount == 3) {
                shader.AddMacro("BLUR_RGB");
            }

            auto fragmentShaderStage = shader.GetStage(AE_FRAGMENT_STAGE);

            auto kernelOffsetsConstant = fragmentShaderStage->GetConstant("offset");
            auto kernelWeightsConstant = fragmentShaderStage->GetConstant("weight");
            auto kernelSizeConstant = fragmentShaderStage->GetConstant("kernelSize");

            kernelOffsetsConstant->SetValue(kernelOffsets, kernelSize);
            kernelWeightsConstant->SetValue(kernelWeights, kernelSize);
            kernelSizeConstant->SetValue(kernelSize);

            shader.Compile();

            GetUniforms();

        }

        void BlurRenderer::Render(Viewport *viewport, RenderTarget *target, Camera *camera, Scene::Scene *scene) {

            return;

        }

        void BlurRenderer::Render(Texture::Texture2D *texture, Texture::Texture2D *swapTexture,
                Texture::Texture2D* depthTexture) {

            framebuffer.Bind();

            if (bilateralBlur) {
                depthTexture->Bind(GL_TEXTURE1);
            }

            glViewport(0, 0, texture->width, texture->height);

            framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, swapTexture);

            texture->Bind(GL_TEXTURE0);

            blurDirection->SetValue(vec2(1.0f / (float)texture->width, 0.0f));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, texture);

            swapTexture->Bind(GL_TEXTURE0);

            blurDirection->SetValue(vec2(0.0f, 1.0f / (float)texture->height));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        }

        void BlurRenderer::GetUniforms() {

            blurDirection = shader.GetUniform("blurDirection");

        }

    }

}