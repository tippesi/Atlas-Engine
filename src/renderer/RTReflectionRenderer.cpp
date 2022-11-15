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

            atrousShader.AddStage(AE_COMPUTE_STAGE, "reflection/atrous.csh");
            atrousShader.Compile();

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
            auto downsampledHistoryRT = target->GetDownsampledHistoryTextures(target->GetReflectionResolution());

            // Should be reflection resolution
            auto depthTexture = downsampledRT->depthTexture;
            auto normalTexture = downsampledRT->geometryNormalTexture;
            auto roughnessTexture = downsampledRT->roughnessMetallicAoTexture;
            auto offsetTexture = downsampledRT->offsetTexture;
            auto velocityTexture = downsampledRT->velocityTexture;
            auto materialIdxTexture = downsampledRT->materialIdxTexture;
            auto randomTexture = &scene->ao->noiseTexture;

            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = downsampledHistoryRT->geometryNormalTexture;

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
                        rtrShader.GetUniform("bias")->SetValue(reflection->bias);

                        rtrShader.GetUniform("frameSeed")->SetValue(Common::Random::SampleUniformInt(0, 255));

                        auto volume = scene->irradianceVolume;
                        if (volume && volume->enable) {
                            auto [irradianceArray, momentsArray] = volume->internal.GetCurrentProbes();
                            irradianceArray.Bind(GL_TEXTURE24);
                            momentsArray.Bind(GL_TEXTURE25);
                            volume->internal.probeStateBuffer.BindBase(14);
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

            Profiler::EndAndBeginQuery("Temporal filter");

            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                temporalShader.Bind();

                target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);
                target->reflectionMomentsTexture.Bind(GL_WRITE_ONLY, 1);

                target->historyReflectionTexture.Bind(GL_TEXTURE0);
                target->swapReflectionTexture.Bind(GL_TEXTURE1);
                velocityTexture->Bind(GL_TEXTURE2);
                depthTexture->Bind(GL_TEXTURE3);
                roughnessTexture->Bind(GL_TEXTURE4);
                offsetTexture->Bind(GL_TEXTURE5);
                normalTexture->Bind(GL_TEXTURE6);
                materialIdxTexture->Bind(GL_TEXTURE7);
                historyMaterialIdxTexture->Bind(GL_TEXTURE8);
                historyNormalTexture->Bind(GL_TEXTURE9);
                target->historyReflectionMomentsTexture.Bind(GL_TEXTURE10);

                temporalShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                temporalShader.GetUniform("invResolution")->SetValue(1.0f / vec2((float)res.x, (float)res.y));
                temporalShader.GetUniform("resolution")->SetValue(vec2((float)res.x, (float)res.y));

                glMemoryBarrier(GL_ALL_BARRIER_BITS);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            target->historyReflectionTexture = target->reflectionTexture;
            target->historyReflectionMomentsTexture = target->reflectionMomentsTexture;

            Profiler::EndAndBeginQuery("Spatial filter");

            {
                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                atrousShader.Bind();

                bool pingpong = true;

                depthTexture->Bind(GL_TEXTURE1);
                normalTexture->Bind(GL_TEXTURE2);

                atrousShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);

                for (int32_t i = 0; i < 4; i++) {
                    Profiler::BeginQuery("Subpass " + std::to_string(i));
                    atrousShader.GetUniform("stepSize")->SetValue(1 << i);

                    if (pingpong) {
                        target->reflectionTexture.Bind(GL_TEXTURE0);
                        target->swapReflectionTexture.Bind(GL_WRITE_ONLY, 0);
                    }
                    else {
                        target->swapReflectionTexture.Bind(GL_TEXTURE0);
                        target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);
                    }

                    pingpong = !pingpong;

                    glMemoryBarrier(GL_ALL_BARRIER_BITS);
                    glDispatchCompute(groupCount.x, groupCount.y, 1);
                    Profiler::EndQuery();
                }
            }            

            Profiler::EndQuery();
            Profiler::EndQuery();

		}

	}

}