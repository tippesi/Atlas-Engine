#include "VolumetricRenderer.h"
#include "../common/RandomHelper.h"

namespace Atlas {

    namespace Renderer {

        void VolumetricRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            blurFilter.CalculateBoxFilter(filterSize);

            std::vector<float> blurKernelWeights;
            std::vector<float> blurKernelOffsets;
            blurFilter.GetLinearized(&blurKernelWeights, &blurKernelOffsets, false);

            auto mean = (blurKernelWeights.size() - 1) / 2;
            blurKernelWeights = std::vector<float>(blurKernelWeights.begin() + mean, blurKernelWeights.end());
            blurKernelOffsets = std::vector<float>(blurKernelOffsets.begin() + mean, blurKernelOffsets.end());

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            volumetricPipelineConfig = PipelineConfig("volumetric/volumetric.csh");
            froxelCullingPipelineConfig = PipelineConfig("volumetric/froxelCulling.csh");
            froxelInjectPipelineConfig = PipelineConfig("volumetric/froxelInject.csh");
            froxelHorizontalFilterPipelineConfig = PipelineConfig("volumetric/froxelFilter.csh", {"HORIZONTAL"});
            froxelVerticalFilterPipelineConfig = PipelineConfig("volumetric/froxelFilter.csh", {"VERTICAL"});
            froxelIntegratePipelineConfig = PipelineConfig("volumetric/froxelIntegrate.csh");
            froxelApplyPipelineConfig = PipelineConfig("volumetric/froxelApply.csh");

            horizontalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"HORIZONTAL", "BLUR_RGBA", "DEPTH_WEIGHT"});
            verticalBlurPipelineConfig = PipelineConfig("bilateralBlur.csh",
                {"VERTICAL", "BLUR_RGBA", "DEPTH_WEIGHT"});

            resolvePipelineConfig = PipelineConfig("volumetric/volumetricResolve.csh");

            lightCullingBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));

            volumetricUniformBuffer = Buffer::UniformBuffer(sizeof(VolumetricUniforms));
            resolveUniformBuffer = Buffer::UniformBuffer(sizeof(ResolveUniforms));
            
            blurWeightsUniformBuffer = Buffer::Buffer(Buffer::BufferUsageBits::UniformBufferBit,
                sizeof(float) * (size_t(filterSize) + 1), 1, blurKernelWeights.data());

            auto samplerDesc = Graphics::SamplerDesc{
                .filter = VK_FILTER_NEAREST,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

            samplerDesc.compareEnabled = false;
            shadowDepthSampler = device->CreateSampler(samplerDesc);

        }

        void VolumetricRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Render volumetric");

            auto renderState = &scene->renderState;
            auto& camera = scene->GetMainCamera();
            auto fog = scene->fog;

            auto lowResDepthTexture = target->GetData(target->GetVolumetricResolution())->depthTexture;
            auto depthTexture = target->GetData(FULL_RES)->depthTexture;

            commandList->ImageMemoryBarrier(target->volumetricTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(target->volumetricTexture.image, 3, 0);
            commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 1);
            renderState->volumetricLightBuffer.Bind(commandList, 3, 8);
            renderState->volumetricShadowBuffer.Bind(commandList, 3, 9);

            ivec2 res = ivec2(target->volumetricTexture.width, target->volumetricTexture.height);

            auto mainLightEntity = GetMainLightEntity(scene);

            VolumetricUniforms uniforms;
            uniforms.sampleCount = fog ? fog->rayMarchStepCount : 1;
            uniforms.intensity = fog ? fog->volumetricIntensity : 0.0f;
            uniforms.lightCount = std::min(128, int32_t(renderState->volumetricLights.size()));
            uniforms.directionalLightCount = 0;
           
            for (const auto& light : renderState->volumetricLights) {                
                auto packedType = reinterpret_cast<const uint32_t&>(light.color.a);
                auto type = static_cast<LightType>(packedType);
               
                if (type != LightType::DirectionalLight)
                    break;

                uniforms.directionalLightCount++;
            }

            const int32_t groupSize = 16;

            ivec2 groupCount = res / groupSize;
            groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            if (lightCullingBuffer.GetElementCount() < groupCount.x * groupCount.y * 64) {
                lightCullingBuffer.SetSize(groupCount.x * groupCount.y * 64);
            }

            auto useFroxels = fog && fog->froxels;

            CullingPushConstants cullingPushConstants;
            cullingPushConstants.lightCount = std::min(4096, int32_t(renderState->volumetricLights.size()));

            lightCullingBuffer.Bind(commandList, 3, 10);

            if (!useFroxels) {
                Graphics::Profiler::BeginQuery("Light culling");

                commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);

                auto cullingPipelineConfig = PipelineConfig("volumetric/lightCulling.csh");
                auto pipeline = PipelineManager::GetPipeline(cullingPipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->PushConstants("constants", &cullingPushConstants);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);

                Graphics::Profiler::EndQuery();
            }

            Graphics::Profiler::BeginQuery(useFroxels ? "Froxels" : "Ray marching");

            auto ocean = scene->ocean;
            bool oceanEnabled = ocean && ocean->enable;

            if (oceanEnabled) {
                // This is the full res depth buffer..
                uniforms.oceanHeight = ocean->translation.y;
                target->oceanDepthTexture.Bind(commandList, 3, 3);
                // Needs barrier
                target->oceanStencilTexture.Bind(commandList, 3, 4);
            }

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
                target->volumetricCloudsTexture.Bind(commandList, 3, 5);

                float cloudInnerRadius = scene->sky.planetRadius + clouds->baseAltitude;
                uniforms.planetCenterAndRadius = vec4(scene->sky.planetCenter, cloudInnerRadius);
            }

            if (cloudShadowsEnabled) {
                auto& mainLight = mainLightEntity.GetComponent<LightComponent>();
                auto& cloudShadowUniform = uniforms.cloudShadow;

                clouds->shadowTexture.Bind(commandList, 3, 2);

                clouds->GetShadowMatrices(camera, normalize(mainLight.transformedProperties.directional.direction),
                    cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera.invViewMatrix;

                cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);
            }
            
            commandList->BindSampler(shadowSampler, 3, 6);
            commandList->BindSampler(shadowDepthSampler, 3, 12);

            volumetricUniformBuffer.SetData(&uniforms, 0);
            commandList->BindBuffer(volumetricUniformBuffer.Get(), 3, 7);

            if (useFroxels) {
                RenderFroxels(target, scene, res, cullingPushConstants.lightCount, cloudsEnabled,
                    cloudShadowsEnabled, oceanEnabled, commandList);
            }
            else {
                volumetricPipelineConfig.ManageMacro("CLOUDS", cloudsEnabled);
                volumetricPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowsEnabled);
                volumetricPipelineConfig.ManageMacro("OCEAN", oceanEnabled);
                volumetricPipelineConfig.ManageMacro("LOCAL_LIGHTS", fogEnabled && fog->localLights);
                auto volumetricPipeline = PipelineManager::GetPipeline(volumetricPipelineConfig);

                commandList->BindPipeline(volumetricPipeline);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            Graphics::Profiler::EndQuery();

            if (fog && fog->enable && fog->rayMarching && !useFroxels) {
                Graphics::Profiler::BeginQuery("Bilateral blur");

                const int32_t blurGroupSize = 256;

                auto kernelSize = int32_t(filterSize);

                auto horizontalBlurPipeline = PipelineManager::GetPipeline(horizontalBlurPipelineConfig);
                auto verticalBlurPipeline = PipelineManager::GetPipeline(verticalBlurPipelineConfig);

                commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 2);
                commandList->BindBuffer(blurWeightsUniformBuffer.Get(), 3, 4);

                for (int32_t j = 0; j < 3; j++) {
                    groupCount = ivec2(res.x / blurGroupSize, res.y);
                    groupCount.x += ((res.x % blurGroupSize == 0) ? 0 : 1);
                    Graphics::ImageBarrier preImageBarriers[] = {
                        {target->volumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {target->swapVolumetricTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(preImageBarriers, {});

                    commandList->BindPipeline(horizontalBlurPipeline);
                    commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                    commandList->BindImage(target->swapVolumetricTexture.image, 3, 0);
                    commandList->BindImage(target->volumetricTexture.image, target->volumetricTexture.sampler, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);

                    groupCount = ivec2(res.x, res.y / blurGroupSize);
                    groupCount.y += ((res.y % blurGroupSize == 0) ? 0 : 1);

                    Graphics::ImageBarrier postImageBarriers[] = {
                        {target->swapVolumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                        {target->volumetricTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    };
                    commandList->PipelineBarrier(postImageBarriers, {});

                    commandList->BindPipeline(verticalBlurPipeline);
                    commandList->PushConstants("constants", &kernelSize, sizeof(int32_t));

                    commandList->BindImage(target->volumetricTexture.image, 3, 0);
                    commandList->BindImage(target->swapVolumetricTexture.image, target->swapVolumetricTexture.sampler, 3, 1);

                    commandList->Dispatch(groupCount.x, groupCount.y, 1);
                }

                Graphics::Profiler::EndQuery();
            }

            Graphics::ImageBarrier imageBarriers[] = {
                {target->volumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, {});

            {
                Graphics::Profiler::BeginQuery("Resolve");

                const int32_t groupSize = 8;

                res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

                groupCount = res / groupSize;
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
                
                if (scene->HasMainLight()) {
                    auto& mainLight = scene->GetMainLight();
                    auto& lightDirection = mainLight.transformedProperties.directional.direction;
                    auto lightColor = Common::ColorConverter::ConvertSRGBToLinear(mainLight.color);

                    uniforms.mainLightColor = vec4(lightColor * mainLight.intensity, 0.0f);
                    uniforms.mainLightDirection = vec4(lightDirection, 0.0f);
                }
                else {
                    uniforms.mainLightColor = vec4(0.0f);
                }

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
                    uniforms.innerCloudRadius = scene->sky.planetRadius + clouds->baseAltitude;
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

        void VolumetricRenderer::RenderFroxels(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
            ivec2 resolution, int32_t lightCount, bool cloudsEnabled, bool cloudShadowsEnabled, bool oceanEnabled,
            Graphics::CommandList* commandList) {

            auto fog = scene->fog;
            auto& camera = scene->GetMainCamera();

            auto tileSize = glm::clamp(fog->froxelTileSize, 4, 32);
            ivec2 columnCount = resolution / tileSize;
            columnCount.x += ((resolution.x % tileSize == 0) ? 0 : 1);
            columnCount.y += ((resolution.y % tileSize == 0) ? 0 : 1);

            auto sliceCount = std::max(fog->froxelSliceCount, 1);
            auto froxelCount = ivec3(columnCount, sliceCount);

            ivec3 clusterCount = froxelCount / froxelClusterSize;
            clusterCount.x += ((froxelCount.x % froxelClusterSize == 0) ? 0 : 1);
            clusterCount.y += ((froxelCount.y % froxelClusterSize == 0) ? 0 : 1);
            clusterCount.z += ((froxelCount.z % froxelClusterSize == 0) ? 0 : 1);

            if (lightCullingBuffer.GetElementCount() < clusterCount.x * clusterCount.y * clusterCount.z * 64) {
                lightCullingBuffer.SetSize(clusterCount.x * clusterCount.y * clusterCount.z * 64);
            }
            lightCullingBuffer.Bind(commandList, 3, 10);

            if (target->froxelTexture.width != columnCount.x || target->froxelTexture.height != columnCount.y ||
                target->froxelTexture.depth != sliceCount)
                target->SetFroxelResolution(froxelCount);

            auto resetHistory = !target->HasHistory() ||
                target->historyFroxelTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED;

            // See "The Unreasonable Effectiveness of Quasirandom Sequences", Roberts 2018
            const double g = 1.22074408460575947536;
            auto r3 = [&](int32_t axis) {
                auto alpha = 1.0 / std::pow(g, double(axis + 1));
                auto value = 0.5 + alpha * double(froxelFrameCount);
                return float(value - std::floor(value));
            };
            auto jitter = vec4(r3(0), r3(1), r3(2), 0.0f);
            froxelFrameCount++;

            FroxelPushConstants constants = {
                .columnCount = columnCount,
                .resolution = resolution,
                .sliceCount = sliceCount,
                .tileSize = tileSize,
                .near = camera.nearPlane,
                .far = fog->froxelFarDistance > camera.nearPlane ?
                    std::min(fog->froxelFarDistance, camera.farPlane) : camera.farPlane,
                .lightCount = lightCount,
                .resetHistory = resetHistory ? 1 : 0,
                .jitter = jitter,
                .extendedFar = camera.farPlane,
                .currentWeight = 1.0f - glm::clamp(fog->froxelHistoryWeight, 0.0f, 0.99f),
                .distributionExponent = glm::clamp(fog->froxelDistributionExponent, 0.1f, 4.0f),
                .directionalJitter = fog->froxelDirectionalJitter ? 1 : 0,
                .localLightJitter = fog->froxelLocalLightJitter ? 1 : 0,
                .localLightSampleCount = glm::clamp(fog->froxelLocalLightSamples, 1, 16),
                .jitterMode = fog->froxelJitterMode
            };

            auto lowResDepthTexture = target->GetData(target->GetVolumetricResolution())->depthTexture;

            {
                Graphics::Profiler::BeginQuery("Light culling");

                auto pipeline = PipelineManager::GetPipeline(froxelCullingPipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);
                commandList->Dispatch(clusterCount.x, clusterCount.y, clusterCount.z);
                commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);
            }

            {
                Graphics::Profiler::EndAndBeginQuery("Inject");

                froxelInjectPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowsEnabled);
                froxelInjectPipelineConfig.ManageMacro("LOCAL_LIGHTS", fog->enable && fog->localLights);
                auto pipeline = PipelineManager::GetPipeline(froxelInjectPipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {target->froxelTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                    {target->historyFroxelTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(target->froxelTexture.image, 3, 0);
                commandList->BindImage(target->historyFroxelTexture.image, target->historyFroxelTexture.sampler, 3, 1);

                commandList->Dispatch(clusterCount.x, clusterCount.y, clusterCount.z);
            }

            auto integrateInputTexture = &target->froxelTexture;
            if (fog->froxelFilter) {
                Graphics::Profiler::EndAndBeginQuery("Filter");

                ivec3 groupCount = froxelCount / 4;
                groupCount.x += ((froxelCount.x % 4 == 0) ? 0 : 1);
                groupCount.y += ((froxelCount.y % 4 == 0) ? 0 : 1);
                groupCount.z += ((froxelCount.z % 4 == 0) ? 0 : 1);

                // The integrated texture is free until the integration, it holds the horizontal pass
                auto pipeline = PipelineManager::GetPipeline(froxelHorizontalFilterPipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {target->froxelTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->integratedFroxelTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(target->integratedFroxelTexture.image, 3, 0);
                commandList->BindImage(target->froxelTexture.image, target->froxelTexture.sampler, 3, 1);

                commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

                pipeline = PipelineManager::GetPipeline(froxelVerticalFilterPipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier verticalImageBarriers[] = {
                    {target->integratedFroxelTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->filteredFroxelTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(verticalImageBarriers, {});

                commandList->BindImage(target->filteredFroxelTexture.image, 3, 0);
                commandList->BindImage(target->integratedFroxelTexture.image, target->integratedFroxelTexture.sampler, 3, 1);

                commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

                integrateInputTexture = &target->filteredFroxelTexture;
            }

            {
                Graphics::Profiler::EndAndBeginQuery("Integrate");

                froxelIntegratePipelineConfig.ManageMacro("CLOUDS", cloudsEnabled);
                auto pipeline = PipelineManager::GetPipeline(froxelIntegratePipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                Graphics::ImageBarrier imageBarriers[] = {
                    {integrateInputTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->integratedFroxelTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(target->integratedFroxelTexture.image, 3, 0);
                commandList->BindImage(integrateInputTexture->image, integrateInputTexture->sampler, 3, 1);

                ivec2 groupCount = columnCount / 8;
                groupCount.x += ((columnCount.x % 8 == 0) ? 0 : 1);
                groupCount.y += ((columnCount.y % 8 == 0) ? 0 : 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            {
                Graphics::Profiler::EndAndBeginQuery("Apply");

                froxelApplyPipelineConfig.ManageMacro("OCEAN", oceanEnabled);
                auto pipeline = PipelineManager::GetPipeline(froxelApplyPipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                commandList->ImageMemoryBarrier(target->integratedFroxelTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

                commandList->BindImage(target->volumetricTexture.image, 3, 0);
                commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 1);
                commandList->BindImage(target->integratedFroxelTexture.image, target->integratedFroxelTexture.sampler, 3, 20);

                ivec2 groupCount = resolution / 8;
                groupCount.x += ((resolution.x % 8 == 0) ? 0 : 1);
                groupCount.y += ((resolution.y % 8 == 0) ? 0 : 1);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            Graphics::Profiler::EndQuery();

            std::swap(target->froxelTexture, target->historyFroxelTexture);

        }

    }

}

