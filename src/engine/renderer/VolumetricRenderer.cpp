#include "VolumetricRenderer.h"
#include "../common/RandomHelper.h"

namespace Atlas {

    namespace Renderer {

        void VolumetricRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            const int32_t filterSize = 4;
            blurFilter.CalculateBoxFilter(filterSize);

            volumetricPipelineConfig = PipelineConfig("volumetric/volumetric.csh");

            horizontalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"HORIZONTAL", "BLUR_RGBA", "DEPTH_WEIGHT"});
            verticalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"VERTICAL", "BLUR_RGBA", "DEPTH_WEIGHT"});

            resolvePipelineConfig = PipelineConfig("volumetric/volumetricResolve.csh");

            volumetricUniformBuffer = Buffer::UniformBuffer(sizeof(VolumetricUniforms));
            resolveUniformBuffer = Buffer::UniformBuffer(sizeof(ResolveUniforms));
            blurWeightsUniformBuffer = Buffer::UniformBuffer(sizeof(float) * (size_t(filterSize) + 1));

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void VolumetricRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Render volumetric");

            auto& camera = scene->GetMainCamera();
            auto fog = scene->fog;

            auto lowResDepthTexture = target->GetData(target->GetVolumetricResolution())->depthTexture;
            auto depthTexture = target->GetData(FULL_RES)->depthTexture;

            commandList->ImageMemoryBarrier(target->volumetricTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(target->volumetricTexture.image, 3, 0);
            commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 1);

            ivec2 res = ivec2(target->volumetricTexture.width, target->volumetricTexture.height);

            Graphics::Profiler::BeginQuery("Ray marching");

            auto mainLightEntity = GetMainLightEntity(scene);
            auto lightSubset = scene->GetSubset<LightComponent>();

            for (auto& lightEntity : lightSubset) {

                auto& light = lightEntity.GetComponent<LightComponent>();
                if (light.type != LightType::DirectionalLight ||
                    !light.volumetric || !fog || !fog->rayMarching)
                    continue;

                auto shadow = light.shadow;

                const int32_t groupSize = 8;

                res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

                ivec2 groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                vec3 direction = normalize(vec3(camera.viewMatrix *
                    vec4(light.transformedProperties.directional.direction, 0.0f)));

                VolumetricUniforms uniforms;
                uniforms.sampleCount = fog->rayMarchStepCount;
                uniforms.intensity = fog->volumetricIntensity * light.intensity;

                uniforms.light.direction = vec4(direction, 0.0);
                uniforms.light.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), 0.0);

                if (light.shadow) {
                    uniforms.light.shadow.cascadeCount = shadow->viewCount;
                    uniforms.light.shadow.edgeSoftness = shadow->edgeSoftness;

                    commandList->BindImage(shadow->maps.image, shadowSampler, 3, 2);

                    auto& shadowUniform = uniforms.light.shadow;
                    for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                        auto& cascadeUniform = shadowUniform.cascades[i];
                        auto cascadeString = "light.shadow.cascades[" + std::to_string(i) + "]";
                        if (i < shadow->viewCount) {
                            auto cascade = &shadow->views[i];
                            cascadeUniform.distance = cascade->farDistance;
                            cascadeUniform.cascadeSpace = cascade->projectionMatrix *
                                cascade->viewMatrix * camera.invViewMatrix;
                        }
                        else {
                            cascadeUniform.distance = camera.farPlane;
                        }
                    }
                }

                auto ocean = scene->ocean;
                bool oceanEnabled = ocean && ocean->enable;

                if (oceanEnabled) {
                    // This is the full res depth buffer..
                    uniforms.oceanHeight = ocean->translation.y;
                    target->oceanDepthTexture.Bind(commandList, 3, 4);
                    // Needs barrier
                    target->oceanStencilTexture.Bind(commandList, 3, 5);
                }

                auto fog = scene->fog;
                bool fogEnabled = fog && fog->enable;

                uniforms.fogEnabled = fogEnabled ? 1 : 0;

                if (fogEnabled) {
                    auto& fogUniform = uniforms.fog;
                    fogUniform.extinctionCoefficient = fog->extinctionCoefficients;
                    fogUniform.scatteringFactor = fog->scatteringFactor;
                    fogUniform.extinctionFactor = fog->extinctionFactor;
                    fogUniform.density = fog->density;
                    fogUniform.heightFalloff = fog->heightFalloff;
                    fogUniform.height = fog->height;
                    fogUniform.ambientFactor = fog->ambientFactor;
                    fogUniform.scatteringAnisotropy = glm::clamp(fog->scatteringAnisotropy, -0.999f, 0.999f);
                }

                auto clouds = scene->sky.clouds;
                bool cloudsEnabled = clouds && clouds->enable && mainLightEntity.IsValid();
                bool cloudShadowsEnabled = cloudsEnabled && clouds->castShadow;

                if (cloudsEnabled) {
                    target->volumetricCloudsTexture.Bind(commandList, 3, 6);

                    float cloudInnerRadius = scene->sky.planetRadius + clouds->minHeight;
                    uniforms.planetCenterAndRadius = vec4(scene->sky.planetCenter, cloudInnerRadius);
                }

                if (cloudShadowsEnabled) {
                    auto& cloudShadowUniform = uniforms.cloudShadow;

                    clouds->shadowTexture.Bind(commandList, 3, 3);

                    clouds->GetShadowMatrices(camera, normalize(light.transformedProperties.directional.direction),
                        cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                    cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera.invViewMatrix;

                    cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                    cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);
                }

                volumetricUniformBuffer.SetData(&uniforms, 0);
                commandList->BindBuffer(volumetricUniformBuffer.Get(), 3, 7);

                volumetricPipelineConfig.ManageMacro("SHADOWS", light.shadow != nullptr);
                volumetricPipelineConfig.ManageMacro("CLOUDS", cloudsEnabled);
                volumetricPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowsEnabled);
                volumetricPipelineConfig.ManageMacro("OCEAN", oceanEnabled);
                auto volumetricPipeline = PipelineManager::GetPipeline(volumetricPipelineConfig);

                commandList->BindPipeline(volumetricPipeline);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            Graphics::Profiler::EndQuery();

            std::vector<Graphics::BufferBarrier> bufferBarriers;
            std::vector<Graphics::ImageBarrier> imageBarriers;

            if (fog && fog->enable && fog->rayMarching) {
                Graphics::Profiler::BeginQuery("Bilateral blur");

                const int32_t groupSize = 256;

                std::vector<float> kernelWeights;
                std::vector<float> kernelOffsets;

                blurFilter.GetLinearized(&kernelWeights, &kernelOffsets, false);

                auto mean = (kernelWeights.size() - 1) / 2;
                kernelWeights = std::vector<float>(kernelWeights.begin() + mean, kernelWeights.end());
                kernelOffsets = std::vector<float>(kernelOffsets.begin() + mean, kernelOffsets.end());

                auto kernelSize = int32_t(kernelWeights.size() - 1);

                auto horizontalBlurPipeline = PipelineManager::GetPipeline(horizontalBlurPipelineConfig);
                auto verticalBlurPipeline = PipelineManager::GetPipeline(verticalBlurPipelineConfig);

                blurWeightsUniformBuffer.SetData(kernelWeights.data(), 0);

                commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 2);
                commandList->BindBuffer(blurWeightsUniformBuffer.Get(), 3, 4);

                ivec2 groupCount = ivec2(res.x / groupSize, res.y);
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                imageBarriers = {
                    {target->volumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->swapVolumetricTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                commandList->BindPipeline(horizontalBlurPipeline);
                commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                commandList->BindImage(target->swapVolumetricTexture.image, 3, 0);
                commandList->BindImage(target->volumetricTexture.image, target->volumetricTexture.sampler, 3, 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                groupCount = ivec2(res.x, res.y / groupSize);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                imageBarriers = {
                    {target->swapVolumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->volumetricTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                commandList->BindPipeline(verticalBlurPipeline);
                commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                commandList->BindImage(target->volumetricTexture.image, 3, 0);
                commandList->BindImage(target->swapVolumetricTexture.image, target->swapVolumetricTexture.sampler, 3, 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
            }

            imageBarriers = {
                {target->volumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

            {
                Graphics::Profiler::BeginQuery("Resolve");

                const int32_t groupSize = 8;

                res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

                ivec2 groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                auto clouds = scene->sky.clouds;
                auto cloudsEnabled = clouds && clouds->enable && mainLightEntity.IsValid();

                auto fog = scene->fog;
                bool fogEnabled = fog && fog->enable;

                resolvePipelineConfig.ManageMacro("CLOUDS", cloudsEnabled);
                resolvePipelineConfig.ManageMacro("FOG", fogEnabled);
                resolvePipelineConfig.ManageMacro("RAYMARCHED_FOG", fogEnabled && fog->rayMarching);

                auto resolvePipeline = PipelineManager::GetPipeline(resolvePipelineConfig);
                commandList->BindPipeline(resolvePipeline);

                commandList->BindImage(target->lightingTexture.image, 3, 0);
                commandList->BindImage(target->volumetricTexture.image, target->volumetricTexture.sampler, 3, 1);
                commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 2);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);

                ResolveUniforms uniforms;
                uniforms.cloudsEnabled = cloudsEnabled ? 1 : 0;
                uniforms.fogEnabled = fogEnabled ? 1 : 0;
                uniforms.downsampled2x = target->GetVolumetricResolution() == RenderResolution::HALF_RES ? 1 : 0;

                if (fogEnabled) {
                    auto& fogUniform = uniforms.fog;
                    fogUniform.extinctionCoefficient = fog->extinctionCoefficients;
                    fogUniform.scatteringFactor = fog->scatteringFactor;
                    fogUniform.extinctionFactor = fog->extinctionFactor;
                    fogUniform.density = fog->density;
                    fogUniform.heightFalloff = fog->heightFalloff;
                    fogUniform.height = fog->height;
                    fogUniform.ambientFactor = fog->ambientFactor;
                    fogUniform.scatteringAnisotropy = glm::clamp(fog->scatteringAnisotropy, -0.999f, 0.999f);
                }

                if (cloudsEnabled) {
                    uniforms.innerCloudRadius = scene->sky.planetRadius + clouds->minHeight;
                    uniforms.planetRadius = scene->sky.planetRadius;
                    uniforms.cloudDistanceLimit = clouds->distanceLimit;
                    uniforms.planetCenter = vec4(scene->sky.planetCenter, 1.0f);
                    commandList->BindImage(target->volumetricCloudsTexture.image, target->volumetricCloudsTexture.sampler, 3, 3);
                }

                resolveUniformBuffer.SetData(&uniforms, 0);
                commandList->BindBuffer(resolveUniformBuffer.Get(), 3, 5);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
            }

            commandList->ImageMemoryBarrier(target->lightingTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

    }

}
