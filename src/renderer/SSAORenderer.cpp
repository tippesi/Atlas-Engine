#include "SSAORenderer.h"

namespace Atlas {

    namespace Renderer {

        SSAORenderer::SSAORenderer() {

            blurFilter.CalculateBoxFilter(3);

            ssaoShader.AddStage(AE_COMPUTE_STAGE, "ao/ssao.csh");

            ssaoShader.Compile();

            horizontalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            horizontalBlurShader.AddMacro("HORIZONTAL");
            horizontalBlurShader.AddMacro("DEPTH_WEIGHT");
            horizontalBlurShader.Compile();

            verticalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            verticalBlurShader.AddMacro("VERTICAL");
            verticalBlurShader.AddMacro("DEPTH_WEIGHT");
            verticalBlurShader.Compile();

            atomicCounterBuffer = Buffer::Buffer(AE_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), 0);
            atomicCounterBuffer.SetSize(1);
            InvalidateCounterBuffer();
        }

        void SSAORenderer::Render(Viewport* viewport, RenderTarget* target,
            Camera* camera, Scene::Scene* scene) {

            auto ssao = scene->ssao;
            if (!ssao || !ssao->enable) return;

            ivec2 res = ivec2(target->ssaoTexture.width, target->ssaoTexture.height);

            // Calculate SSAO
            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
                groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

                ssaoShader.Bind();

                ssaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                ssaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);

                ssaoShader.GetUniform("sampleCount")->SetValue(ssao->sampleCount);
                ssaoShader.GetUniform("samples")->SetValue(ssao->samples.data(), int32_t(ssao->samples.size()));
                ssaoShader.GetUniform("radius")->SetValue(ssao->radius);
                ssaoShader.GetUniform("strength")->SetValue(ssao->strength);
                ssaoShader.GetUniform("resolution")->SetValue(vec2(res));

                // Bind the geometry normal texure and depth texture
                target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE1);
                target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE2);

                ssao->noiseTexture.Bind(GL_TEXTURE3);

                target->ssaoTexture.Bind(GL_WRITE_ONLY, 0);
                atomicCounterBuffer.BindBase(0);

                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            {
                const int32_t groupSize = 256;

                target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets, false);

                ivec2 groupCount = ivec2(res.x / groupSize, res.y);
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);

                horizontalBlurShader.Bind();

                horizontalBlurShader.GetUniform("weights")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                horizontalBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size() - 1);

                target->ssaoTexture.Bind(GL_TEXTURE0);
                target->swapSsaoTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);

                groupCount = ivec2(res.x, res.y / groupSize);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                verticalBlurShader.Bind();

                verticalBlurShader.GetUniform("weights")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                verticalBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size() - 1);

                target->swapSsaoTexture.Bind(GL_TEXTURE0);
                target->ssaoTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }
            
            InvalidateCounterBuffer();
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        void SSAORenderer::InvalidateCounterBuffer() {

            uint32_t zero = 0;
            atomicCounterBuffer.Bind();
            atomicCounterBuffer.InvalidateData();
            atomicCounterBuffer.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);

        }

    }

}