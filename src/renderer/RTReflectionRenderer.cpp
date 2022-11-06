#include "RTReflectionRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"
#include "loader/ImageLoader.h"

namespace Atlas {

	namespace Renderer {

        RTReflectionRenderer::RTReflectionRenderer() {

            const int32_t filterSize = 2;
            blurFilter.CalculateBoxFilter(filterSize);
            
            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("noise.png");
            blueNoiseTexture = Texture::Texture2D(noiseImage.width, noiseImage.height, GL_RGBA8, GL_REPEAT, GL_NEAREST);
            blueNoiseTexture.SetData(noiseImage.GetData());

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

            auto reflection = scene->reflection;
            if (!reflection || !reflection->enable) return;

            helper.SetScene(scene, 8, false);
            helper.UpdateLights();

            ivec2 res = ivec2(target->aoTexture.width, target->aoTexture.height);

            Profiler::BeginQuery("Render RT Reflections");
            Profiler::BeginQuery("Trace rays");

            auto downsampledRT = target->GetDownsampledTextures(target->GetReflectionResolution());

            // Should be reflection resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT4);
            auto randomTexture = &scene->ao->noiseTexture;

            // Bind the geometry normal texure and depth texture
            normalTexture->Bind(GL_TEXTURE16);
            depthTexture->Bind(GL_TEXTURE17);
            roughnessTexture->Bind(GL_TEXTURE18);
            offsetTexture->Bind(GL_TEXTURE19);
            materialIdxTexture->Bind(GL_TEXTURE20);
            blueNoiseTexture.Bind(GL_TEXTURE21);

            // Calculate RTAO
            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);
                
                rtrShader.ManageMacro("USE_SHADOW_MAP", reflection->useShadowMap);
                rtrShader.ManageMacro("GI", reflection->gi);

                helper.DispatchAndHit(&rtrShader, ivec3(groupCount, 1),
                    [=]() {
                        target->swapReflectionTexture.Bind(GL_WRITE_ONLY, 4);                        

                        rtrShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
                        rtrShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                        rtrShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

                        rtrShader.GetUniform("sampleCount")->SetValue(reflection->sampleCount);
                        rtrShader.GetUniform("radianceLimit")->SetValue(reflection->radianceLimit);
                        rtrShader.GetUniform("useShadowMap")->SetValue(reflection->useShadowMap);
                        rtrShader.GetUniform("resolution")->SetValue(res);

                        rtrShader.GetUniform("jitter")->SetValue(camera->GetJitter());

                        rtrShader.GetUniform("frameSeed")->SetValue(Common::Random::SampleUniformInt(0, 255));

                        auto volume = scene->irradianceVolume;
                        if (volume && volume->enable) {
                            auto [irradianceArray, momentsArray] = volume->internal.GetCurrentProbes();
                            irradianceArray.Bind(GL_TEXTURE24);
                            momentsArray.Bind(GL_TEXTURE25);
                            volume->internal.probeStateBuffer.BindBase(1);
                            rtrShader.GetUniform("volumeEnabled")->SetValue(true);
                            rtrShader.GetUniform("volumeMin")->SetValue(volume->aabb.min);
                            rtrShader.GetUniform("volumeMax")->SetValue(volume->aabb.max);
                            rtrShader.GetUniform("volumeProbeCount")->SetValue(volume->probeCount);
                            rtrShader.GetUniform("volumeIrradianceRes")->SetValue(volume->irrRes);
                            rtrShader.GetUniform("volumeMomentsRes")->SetValue(volume->momRes);
                            rtrShader.GetUniform("volumeBias")->SetValue(volume->bias);
                            rtrShader.GetUniform("volumeGamma")->SetValue(volume->gamma);
                            rtrShader.GetUniform("cellSize")->SetValue(volume->cellSize);
                            rtrShader.GetUniform("indirectStrength")->SetValue(volume->strength);
                        }
                        else {
                            rtrShader.GetUniform("volumeEnabled")->SetValue(false);
                            rtrShader.GetUniform("volumeMin")->SetValue(vec3(0.0f));
                            rtrShader.GetUniform("volumeMax")->SetValue(vec3(0.0f));
                        }
                    });
            }

            Profiler::EndAndBeginQuery("Temporal accumulation");

            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                temporalShader.Bind();

                target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);

                target->historyReflectionTexture.Bind(GL_TEXTURE0);
                target->swapReflectionTexture.Bind(GL_TEXTURE1);
                velocityTexture->Bind(GL_TEXTURE2);
                depthTexture->Bind(GL_TEXTURE3);
                roughnessTexture->Bind(GL_TEXTURE4);
                offsetTexture->Bind(GL_TEXTURE5);
                materialIdxTexture->Bind(GL_TEXTURE6);

                temporalShader.GetUniform("invResolution")->SetValue(1.0f / vec2((float)res.x, (float)res.y));
                temporalShader.GetUniform("resolution")->SetValue(vec2((float)res.x, (float)res.y));

                glMemoryBarrier(GL_ALL_BARRIER_BITS);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            target->historyReflectionTexture = target->reflectionTexture;

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