#include "RTReflectionRenderer.h"

#include "Clock.h"

namespace Atlas {

	namespace Renderer {

        RTReflectionRenderer::RTReflectionRenderer() {

            const int32_t filterSize = 1;
            blurFilter.CalculateBoxFilter(filterSize);

            rtrShader.AddStage(AE_COMPUTE_STAGE, "reflection/rtreflection.csh");
            rtrShader.Compile();

            temporalShader.AddStage(AE_COMPUTE_STAGE, "reflection/temporal.csh");
            temporalShader.Compile();

            horizontalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            horizontalBlurShader.AddMacro("HORIZONTAL");
            horizontalBlurShader.AddMacro("DEPTH_WEIGHT");
            horizontalBlurShader.AddMacro("BLUR_RGB");
            horizontalBlurShader.Compile();

            verticalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            verticalBlurShader.AddMacro("VERTICAL");
            verticalBlurShader.AddMacro("DEPTH_WEIGHT");
            verticalBlurShader.AddMacro("BLUR_RGB");
            verticalBlurShader.Compile();

		}

		void RTReflectionRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            //auto ssao = scene->ssao;
            //if (!ssao || !ssao->enable) return;

            helper.SetScene(scene, 8, false);
            helper.UpdateLights();

            ivec2 res = ivec2(target->aoTexture.width, target->aoTexture.height);

            Profiler::BeginQuery("Render RT Reflections");
            Profiler::BeginQuery("Trace rays");

            // Should be reflection resolution
            auto depthTexture = target->GetDownsampledDepthTexture(target->GetReflectionResolution());
            auto normalTexture = target->GetDownsampledNormalTexture(target->GetReflectionResolution());
            auto roughnessTexture = target->GetDownsampledRoughnessMetalnessAoTexture(target->GetReflectionResolution());
            auto offsetTexture = target->GetDownsampledOffsetTexture(target->GetReflectionResolution());
            auto materialIdxTexture = target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT4);
            auto randomTexture = &scene->ssao->noiseTexture;

            auto history = target->reflectionTexture;

            // Bind the geometry normal texure and depth texture
            normalTexture->Bind(GL_TEXTURE16);
            depthTexture->Bind(GL_TEXTURE17);
            roughnessTexture->Bind(GL_TEXTURE18);
            offsetTexture->Bind(GL_TEXTURE19);
            materialIdxTexture->Bind(GL_TEXTURE20);
            randomTexture->Bind(GL_TEXTURE21);

            // Calculate RTAO
            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                helper.DispatchAndHit(&rtrShader, ivec3(groupCount, 1),
                    [=]() {
                        target->swapReflectionTexture.Bind(GL_WRITE_ONLY, 4);                        

                        rtrShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                        rtrShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                        rtrShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

                        //rtaoShader.GetUniform("sampleCount")->SetValue(ssao->sampleCount);
                        //rtaoShader.GetUniform("radius")->SetValue(ssao->radius);
                        rtrShader.GetUniform("resolution")->SetValue(res);

                        rtrShader.GetUniform("frameSeed")->SetValue(Clock::Get());
                    });
            }

            Profiler::EndAndBeginQuery("Temporal accumulation");

            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                temporalShader.Bind();

                target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);

                history.Bind(GL_TEXTURE0);
                target->swapReflectionTexture.Bind(GL_TEXTURE1);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
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

                target->reflectionTexture.Bind(GL_TEXTURE0);
                target->swapReflectionTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);

                groupCount = ivec2(res.x, res.y / groupSize);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                verticalBlurShader.Bind();

                verticalBlurShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                verticalBlurShader.GetUniform("weights")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                verticalBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size() - 1);

                target->swapReflectionTexture.Bind(GL_TEXTURE0);
                target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            Profiler::EndQuery();
            Profiler::EndQuery();

		}

	}

}