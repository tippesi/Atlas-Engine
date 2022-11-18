#include "AORenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"

namespace Atlas {

	namespace Renderer {

		AORenderer::AORenderer() {

            const int32_t filterSize = 4;
            blurFilter.CalculateGaussianFilter(float(filterSize) / 3.0f, filterSize);

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("noise.png");
            blueNoiseTexture = Texture::Texture2D(noiseImage.width, noiseImage.height, GL_RGBA8, GL_REPEAT, GL_NEAREST);
            blueNoiseTexture.SetData(noiseImage.GetData());

            ssaoShader.AddStage(AE_COMPUTE_STAGE, "ao/ssao.csh");
            ssaoShader.Compile();

            rtaoShader.AddStage(AE_COMPUTE_STAGE, "ao/rtao.csh");
            rtaoShader.Compile();

            temporalShader.AddStage(AE_COMPUTE_STAGE, "ao/temporal.csh");
            temporalShader.Compile();

            horizontalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            horizontalBlurShader.AddMacro("HORIZONTAL");
            horizontalBlurShader.AddMacro("DEPTH_WEIGHT");
            horizontalBlurShader.AddMacro("NORMAL_WEIGHT");
            horizontalBlurShader.Compile();

            verticalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            verticalBlurShader.AddMacro("VERTICAL");
            verticalBlurShader.AddMacro("DEPTH_WEIGHT");
            verticalBlurShader.AddMacro("NORMAL_WEIGHT");
            verticalBlurShader.Compile();

		}

		void AORenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            auto ao = scene->ao;
            if (!ao || !ao->enable) return;

            helper.SetScene(scene, 8);

            ivec2 res = ivec2(target->aoTexture.width, target->aoTexture.height);

            Profiler::BeginQuery("Render AO");            

            auto downsampledRT = target->GetDownsampledTextures(target->GetAOResolution());
            auto downsampledHistoryRT = target->GetDownsampledHistoryTextures(target->GetAOResolution());

            // Should be AO resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;
            auto randomTexture = &scene->ao->noiseTexture;

            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            // Calculate RTAO
            if (ao->rt) {
                Profiler::BeginQuery("Trace rays/calculate ao");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                helper.DispatchAndHit(&rtaoShader, ivec3(groupCount.x * groupCount.y, 1, 1), 
                    [=]() {
                        target->swapAoTexture.Bind(GL_WRITE_ONLY, 3);

                        // Bind the geometry normal texure and depth texture
                        normalTexture->Bind(GL_TEXTURE0);
                        depthTexture->Bind(GL_TEXTURE1);

                        //ssao->noiseTexture.Bind(GL_TEXTURE2);
                        offsetTexture->Bind(GL_TEXTURE3);
                        blueNoiseTexture.Bind(GL_TEXTURE2);

                        rtaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                        rtaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                        rtaoShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

                        rtaoShader.GetUniform("sampleCount")->SetValue(ao->sampleCount);
                        rtaoShader.GetUniform("radius")->SetValue(ao->radius);
                        rtaoShader.GetUniform("resolution")->SetValue(res);

                        rtaoShader.GetUniform("frameSeed")->SetValue(Common::Random::SampleUniformInt(0, 255));
                    });
            }
            else {
                Profiler::BeginQuery("Main pass");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
                groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

                ssaoShader.Bind();

                ssaoShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                ssaoShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);

                ssaoShader.GetUniform("sampleCount")->SetValue(ao->sampleCount);
                ssaoShader.GetUniform("samples")->SetValue(ao->samples.data(), int32_t(ao->samples.size()));
                ssaoShader.GetUniform("radius")->SetValue(ao->radius);
                ssaoShader.GetUniform("strength")->SetValue(ao->strength);
                ssaoShader.GetUniform("resolution")->SetValue(vec2(res));
                ssaoShader.GetUniform("frameCount")->SetValue(0);

                // Bind the geometry normal texure and depth texture
                normalTexture->Bind(GL_TEXTURE1);
                depthTexture->Bind(GL_TEXTURE2);

                ao->noiseTexture.Bind(GL_TEXTURE3);

                target->swapAoTexture.Bind(GL_WRITE_ONLY, 0);

                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            if (ao->rt) {
                Profiler::EndAndBeginQuery("Temporal filter");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                temporalShader.Bind();

                target->aoTexture.Bind(GL_WRITE_ONLY, 0);
                target->aoMomentsTexture.Bind(GL_WRITE_ONLY, 1);

                target->historyAoTexture.Bind(GL_TEXTURE0);
                target->swapAoTexture.Bind(GL_TEXTURE1);
                velocityTexture->Bind(GL_TEXTURE2);
                depthTexture->Bind(GL_TEXTURE3);
                normalTexture->Bind(GL_TEXTURE6);
                materialIdxTexture->Bind(GL_TEXTURE7);
                target->historyAoMomentsTexture.Bind(GL_TEXTURE10);

                temporalShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                temporalShader.GetUniform("invResolution")->SetValue(1.0f / vec2((float)res.x, (float)res.y));
                temporalShader.GetUniform("resolution")->SetValue(vec2((float)res.x, (float)res.y));

                glMemoryBarrier(GL_ALL_BARRIER_BITS);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }
            
            target->historyAoTexture = target->aoTexture;
            target->historyAoMomentsTexture = target->aoMomentsTexture;

            {
                Profiler::EndAndBeginQuery("Blur");

                const int32_t groupSize = 256;

                depthTexture->Bind(GL_TEXTURE1);
                normalTexture->Bind(GL_TEXTURE2);

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets, false);

                auto mean = (kernelWeights.size() - 1) / 2;
                kernelWeights = std::vector<float>(kernelWeights.begin() + mean, kernelWeights.end());
                kernelOffsets = std::vector<float>(kernelOffsets.begin() + mean, kernelOffsets.end());

                for (int32_t i = 0; i < 5; i++) {
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
            }
            
            Profiler::EndQuery();
            Profiler::EndQuery();

		}

	}

}