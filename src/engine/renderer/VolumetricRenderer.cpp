#include "VolumetricRenderer.h"
#include "../lighting/DirectionalLight.h"
#include "../common/RandomHelper.h"

namespace Atlas {

    namespace Renderer {

        VolumetricRenderer::VolumetricRenderer() {

            const int32_t filterSize = 4;
            blurFilter.CalculateGaussianFilter(float(filterSize) / 3.0f, filterSize);

            volumetricShader.AddStage(AE_COMPUTE_STAGE, "volumetric/volumetric.csh");
            volumetricShader.Compile();

            horizontalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            horizontalBlurShader.AddMacro("HORIZONTAL");
            horizontalBlurShader.AddMacro("BLUR_RGB");
            horizontalBlurShader.AddMacro("DEPTH_WEIGHT");
            horizontalBlurShader.Compile();

            verticalBlurShader.AddStage(AE_COMPUTE_STAGE, "bilateralBlur.csh");
            verticalBlurShader.AddMacro("VERTICAL");
            verticalBlurShader.AddMacro("BLUR_RGB");
            verticalBlurShader.AddMacro("DEPTH_WEIGHT");
            verticalBlurShader.Compile();

            volumetricResolveShader.AddStage(AE_COMPUTE_STAGE, "volumetric/volumetricResolve.csh");
            volumetricResolveShader.Compile();

        }

        void VolumetricRenderer::Render(Viewport* viewport, RenderTarget* target,
            Camera* camera, Scene::Scene* scene) {

            Profiler::BeginQuery("Render volumetric");

            volumetricShader.Bind();

            volumetricShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
            volumetricShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

            auto depthTexture = target->GetDownsampledTextures(target->GetVolumetricResolution())->depthTexture;
            depthTexture->Bind(0);

            auto lights = scene->GetLights();

            ivec2 res = ivec2(target->volumetricTexture.width, target->volumetricTexture.height);

            target->volumetricTexture.Bind(GL_WRITE_ONLY, 2);

            Profiler::BeginQuery("Raymarching");

            // This loop doesn't really work, we only support one directional light for now.
            // Later we want to process most of the lights all at once using tiled deferred shading
            for (auto& light : lights) {
                const int32_t groupSize = 8;

                res = ivec2(target->GetWidth(), target->GetHeight());

                ivec2 groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                auto volumetric = light->GetVolumetric();
                auto shadow = light->GetShadow();

                if (light->type != AE_DIRECTIONAL_LIGHT || !volumetric || !shadow) continue;

                auto directionalLight = (Lighting::DirectionalLight*)light;
                vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

                volumetricShader.GetUniform("light.direction")->SetValue(direction);
                volumetricShader.GetUniform("light.color")->SetValue(light->color);
                volumetricShader.GetUniform("light.shadow.cascadeCount")->SetValue(shadow->componentCount);
                volumetricShader.GetUniform("sampleCount")->SetValue(volumetric->sampleCount);
                volumetricShader.GetUniform("framebufferResolution")->SetValue(vec2(res));
                volumetricShader.GetUniform("intensity")->SetValue(volumetric->intensity * light->intensity);
                volumetricShader.GetUniform("seed")->SetValue(Common::Random::SampleFastUniformFloat());

                auto fog = scene->fog;
                bool fogEnabled = fog && fog->enable;

                if (fogEnabled) {
                    volumetricShader.GetUniform("fogEnabled")->SetValue(true);
                    volumetricShader.GetUniform("fogDensity")->SetValue(fog->density);
                    volumetricShader.GetUniform("fogHeightFalloff")->SetValue(fog->heightFalloff);
                    volumetricShader.GetUniform("fogHeight")->SetValue(fog->height);
                    volumetricShader.GetUniform("fogScatteringAnisotropy")->SetValue(glm::clamp(fog->scatteringAnisotropy, -0.999f, 0.999f));
                }
                else {
                    volumetricShader.GetUniform("fogEnabled")->SetValue(false);
                }

                light->GetShadow()->maps.Bind(1);

                for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
                    auto cascadeString = "light.shadow.cascades[" + std::to_string(i) + "]";
                    if (i < shadow->componentCount) {
                        auto cascade = &shadow->components[i];
                        volumetricShader.GetUniform(cascadeString + ".distance")->SetValue(cascade->farDistance);
                        volumetricShader.GetUniform(cascadeString + ".cascadeSpace")->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix);
                    }
                    else {
                        volumetricShader.GetUniform(cascadeString + ".distance")->SetValue(camera->farPlane);
                    }
                }

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);
            }

            Profiler::EndQuery();

            {
                Profiler::BeginQuery("Bilateral blur");

                const int32_t groupSize = 256;

                depthTexture->Bind(1);

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

                target->volumetricTexture.Bind(0);
                target->swapVolumetricTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);

                groupCount = ivec2(res.x, res.y / groupSize);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                verticalBlurShader.Bind();

                verticalBlurShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                verticalBlurShader.GetUniform("weights")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
                verticalBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size() - 1);

                target->swapVolumetricTexture.Bind(0);
                target->volumetricTexture.Bind(GL_WRITE_ONLY, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);

                Profiler::EndQuery();
            }

            {
                Profiler::BeginQuery("Resolve");

                const int32_t groupSize = 8;

                res = ivec2(target->GetWidth(), target->GetHeight());

                ivec2 groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                volumetricResolveShader.Bind();

                // We keep the depth texture binding from blur pass and only bind volumetric texture
                target->volumetricTexture.Bind(0);
                target->GetDownsampledTextures(target->GetVolumetricResolution())->depthTexture->Bind(1);
                target->volumetricCloudsTexture.Bind(2);
                target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(3);

                auto fog = scene->fog;
                bool fogEnabled = fog && fog->enable;

                if (fogEnabled) {
                    volumetricResolveShader.GetUniform("fogEnabled")->SetValue(true);
                    volumetricResolveShader.GetUniform("fogColor")->SetValue(fog->color);
                    volumetricResolveShader.GetUniform("fogDensity")->SetValue(fog->density);
                    volumetricResolveShader.GetUniform("fogHeightFalloff")->SetValue(fog->heightFalloff);
                    volumetricResolveShader.GetUniform("fogHeight")->SetValue(fog->height);
                    volumetricResolveShader.GetUniform("fogScatteringAnisotropy")->SetValue(glm::clamp(fog->scatteringAnisotropy, -0.999f, 0.999f));
                }
                else {
                    volumetricResolveShader.GetUniform("fogEnabled")->SetValue(false);
                }

                volumetricResolveShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);
                volumetricResolveShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
                volumetricResolveShader.GetUniform("cameraLocation")->SetValue(camera->location);
                volumetricResolveShader.GetUniform("downsampled2x")->SetValue(target->GetVolumetricResolution() == RenderResolution::HALF_RES);

                target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_READ_WRITE, 0);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glDispatchCompute(groupCount.x, groupCount.y, 1);

                Profiler::EndQuery();
            }

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            Profiler::EndQuery();

        }

    }

}