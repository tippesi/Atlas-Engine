#include "RTReflectionRenderer.h"

#include "Clock.h"
#include "../common/RandomHelper.h"
#include "loader/ImageLoader.h"

namespace Atlas {

	namespace Renderer {

        RTReflectionRenderer::RTReflectionRenderer() {

            /*
            const int32_t filterSize = 2;
            blurFilter.CalculateBoxFilter(filterSize);
            
            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("noise.png");
            blueNoiseTexture = Texture::Texture2D(noiseImage.width, noiseImage.height, GL_RGBA8, GL_REPEAT, GL_NEAREST);
            blueNoiseTexture.SetData(noiseImage.GetData());

            rtrShader.AddStage(AE_COMPUTE_STAGE, "reflection/rtreflection.csh");
            rtrShader.Compile();

            temporalShader.AddStage(AE_COMPUTE_STAGE, "reflection/temporal.csh");
            temporalShader.Compile();

            atrousShader[0].AddStage(AE_COMPUTE_STAGE, "reflection/atrous.csh");
            atrousShader[0].AddMacro("STEP_SIZE1");
            atrousShader[0].Compile();

            atrousShader[1].AddStage(AE_COMPUTE_STAGE, "reflection/atrous.csh");
            atrousShader[1].AddMacro("STEP_SIZE2");
            atrousShader[1].Compile();

            atrousShader[2].AddStage(AE_COMPUTE_STAGE, "reflection/atrous.csh");
            atrousShader[2].AddMacro("STEP_SIZE4");
            atrousShader[2].Compile();
             */

		}

		void RTReflectionRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            /*
            auto reflection = scene->reflection;
            if (!reflection || !reflection->enable) return;

            helper.SetScene(scene, 8, false);
            helper.UpdateLights();

            ivec2 res = ivec2(target->aoTexture.width, target->aoTexture.height);

            Profiler::BeginQuery("Render RT Reflections");
            Profiler::BeginQuery("Trace rays");

            // Try to get a shadow map
            Lighting::Shadow* shadow = nullptr;
            if (!scene->sky.sun) {
                auto lights = scene->GetLights();
                for (auto& light : lights) {
                    if (light->type == AE_DIRECTIONAL_LIGHT) {
                        shadow = light->GetShadow();
                    }
                }
            }
            else {
                shadow = scene->sky.sun->GetShadow();
            }

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

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            auto historyMaterialIdxTexture = downsampledHistoryRT->materialIdxTexture;
            auto historyNormalTexture = downsampledHistoryRT->geometryNormalTexture;

            // Bind the geometry normal texure and depth texture
            normalTexture->Bind(16);
            depthTexture->Bind(17);
            roughnessTexture->Bind(18);
            offsetTexture->Bind(19);
            materialIdxTexture->Bind(20);
            blueNoiseTexture.Bind(21);

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
                            irradianceArray.Bind(24);
                            momentsArray.Bind(25);
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

                        if (shadow && reflection->useShadowMap) {
                            auto distance = shadow->longRange ? shadow->distance : shadow->longRangeDistance;

                            rtrShader.GetUniform("shadow.distance")->SetValue(distance);
                            rtrShader.GetUniform("shadow.bias")->SetValue(shadow->bias);
                            rtrShader.GetUniform("shadow.cascadeCount")->SetValue(shadow->componentCount);
                            rtrShader.GetUniform("shadow.resolution")->SetValue(vec2((float)shadow->resolution));

                            shadow->maps.Bind(26);

                            for (int32_t i = 0; i < shadow->componentCount; i++) {
                                auto cascade = &shadow->components[i];
                                auto frustum = Volume::Frustum(cascade->frustumMatrix);
                                auto corners = frustum.GetCorners();
                                auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                                    abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                                auto lightSpace = cascade->projectionMatrix * cascade->viewMatrix;
                                rtrShader.GetUniform("shadow.cascades[" + std::to_string(i) + "].distance")->SetValue(cascade->farDistance);
                                rtrShader.GetUniform("shadow.cascades[" + std::to_string(i) + "].cascadeSpace")->SetValue(lightSpace);
                                rtrShader.GetUniform("shadow.cascades[" + std::to_string(i) + "].texelSize")->SetValue(texelSize);
                            }
                        }
                        else {
                            rtrShader.GetUniform("shadow.distance")->SetValue(0.0f);
                        }
                    });
            }

            Profiler::EndAndBeginQuery("Temporal filter");

            {
                ivec2 groupCount = ivec2(res.x / 16, res.y / 16);
                groupCount.x += ((groupCount.x * 16 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == res.y) ? 0 : 1);

                temporalShader.Bind();

                target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);
                target->reflectionMomentsTexture.Bind(GL_WRITE_ONLY, 1);

                target->swapReflectionTexture.Bind(0);
                velocityTexture->Bind(1);
                depthTexture->Bind(2);
                roughnessTexture->Bind(3);
                normalTexture->Bind(4);
                materialIdxTexture->Bind(5);

                target->historyReflectionTexture.Bind(6);
                target->historyReflectionMomentsTexture.Bind(7);
                historyDepthTexture->Bind(8);
                historyNormalTexture->Bind(9);
                historyMaterialIdxTexture->Bind(10);

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
                ivec2 groupCount = ivec2(res.x / 16, res.y / 16);
                groupCount.x += ((groupCount.x * 16 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 16 == res.y) ? 0 : 1);

                

                bool pingpong = true;

                depthTexture->Bind(1);
                normalTexture->Bind(2);
                roughnessTexture->Bind(3);

                for (int32_t i = 0; i < 3; i++) {
                    Profiler::BeginQuery("Subpass " + std::to_string(i));

                    atrousShader[i].Bind();

                    atrousShader[i].GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                    atrousShader[i].GetUniform("resolution")->SetValue(res);
                    atrousShader[i].GetUniform("strength")->SetValue(reflection->spatialFilterStrength);

                    atrousShader[i].GetUniform("stepSize")->SetValue(1 << i);

                    if (pingpong) {
                        target->reflectionTexture.Bind(0);
                        target->swapReflectionTexture.Bind(GL_WRITE_ONLY, 0);
                    }
                    else {
                        target->swapReflectionTexture.Bind(0);
                        target->reflectionTexture.Bind(GL_WRITE_ONLY, 0);
                    }

                    pingpong = !pingpong;

                    glMemoryBarrier(GL_ALL_BARRIER_BITS);
                    glDispatchCompute(groupCount.x, groupCount.y, 1);
                    Profiler::EndQuery();
                }

                if (!pingpong) {
                    glMemoryBarrier(GL_ALL_BARRIER_BITS);
                    target->reflectionTexture = target->swapReflectionTexture;
                }
            }

            Profiler::EndQuery();
            Profiler::EndQuery();
             */

		}

	}

}