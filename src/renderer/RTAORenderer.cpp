#include "RTAORenderer.h"

#include "Clock.h"

namespace Atlas {

	namespace Renderer {

		RTAORenderer::RTAORenderer() {

            const int32_t filterSize = 3;
            blurFilter.CalculateGaussianFilter(float(filterSize) / 3.0f, filterSize);
            blurFilter.CalculateBoxFilter(filterSize);

            rtaoShader.AddStage(AE_COMPUTE_STAGE, "ao/rtao.csh");
            rtaoShader.Compile();

            horizontalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            horizontalBlurShader.AddMacro("HORIZONTAL");
            horizontalBlurShader.AddMacro("DEPTH_WEIGHT");
            horizontalBlurShader.Compile();

            verticalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            verticalBlurShader.AddMacro("VERTICAL");
            verticalBlurShader.AddMacro("DEPTH_WEIGHT");
            verticalBlurShader.Compile();

		}

		void RTAORenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            auto ssao = scene->ao;
            if (!ssao || !ssao->enable) return;

            helper.SetScene(scene, 8);

            ivec2 res = ivec2(target->aoTexture.width, target->aoTexture.height);

            Profiler::BeginQuery("Render RTAO");
            Profiler::BeginQuery("Trace rays/calculate ao");

            auto downsampledRT = target->GetDownsampledTextures(target->GetAOResolution());

            // Should be reflection resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->geometryNormalTexture;
            auto offsetTexture = downsampledRT->offsetTexture;

            // Calculate RTAO
            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                helper.DispatchAndHit(&rtaoShader, ivec3(groupCount.x * groupCount.y, 1, 1), 
                    [=]() {
                        target->aoTexture.Bind(GL_WRITE_ONLY, 3);

                        // Bind the geometry normal texure and depth texture
                        normalTexture->Bind(GL_TEXTURE0);
                        depthTexture->Bind(GL_TEXTURE1);

                        ssao->noiseTexture.Bind(GL_TEXTURE2);
                        offsetTexture->Bind(GL_TEXTURE3);

                        rtaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                        rtaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                        rtaoShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

                        rtaoShader.GetUniform("sampleCount")->SetValue(ssao->sampleCount);
                        rtaoShader.GetUniform("radius")->SetValue(ssao->radius);
                        rtaoShader.GetUniform("resolution")->SetValue(res);

                        rtaoShader.GetUniform("frameSeed")->SetValue(Clock::Get());
                    });
            }

            Profiler::EndAndBeginQuery("Blur");

            framebuffer.Bind();

            {
                const int32_t groupSize = 256;

                depthTexture->Bind(GL_TEXTURE1);

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets, false);

                auto mean = (kernelWeights.size() - 1) / 2;
                kernelWeights = std::vector<float>(kernelWeights.begin() + mean, kernelWeights.end());
                kernelOffsets = std::vector<float>(kernelOffsets.begin() + mean, kernelOffsets.end());

                ivec2 groupCount = ivec2(res.x / groupSize, res.y);
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);

                horizontalBlurShader.Bind();

                horizontalBlurShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                horizontalBlurShader.GetUniform("weights")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                horizontalBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size() - 1);

                target->aoTexture.Bind(GL_TEXTURE0);
                target->swapAoTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);

                groupCount = ivec2(res.x, res.y / groupSize);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                verticalBlurShader.Bind();

                verticalBlurShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                verticalBlurShader.GetUniform("weights")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                verticalBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size() - 1);

                target->swapAoTexture.Bind(GL_TEXTURE0);
                target->aoTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            Profiler::EndQuery();
            Profiler::EndQuery();

		}

	}

}