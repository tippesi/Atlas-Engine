#include "VolumetricRenderer.h"

namespace Atlas {

    namespace Renderer {

        void VolumetricRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            froxelCullingPipelineConfig = PipelineConfig("volumetric/froxelCulling.csh");
            froxelInjectPipelineConfig = PipelineConfig("volumetric/froxelInject.csh");
            froxelHorizontalFilterPipelineConfig = PipelineConfig("volumetric/froxelFilter.csh", {"HORIZONTAL"});
            froxelVerticalFilterPipelineConfig = PipelineConfig("volumetric/froxelFilter.csh", {"VERTICAL"});
            froxelIntegratePipelineConfig = PipelineConfig("volumetric/froxelIntegrate.csh");
            froxelApplyPipelineConfig = PipelineConfig("volumetric/froxelApply.csh");

            resolvePipelineConfig = PipelineConfig("volumetric/volumetricResolve.csh");

            lightCullingBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));
            fogVolumeMaskBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));
            fogVolumeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                | Buffer::BufferUsageBits::StorageBufferBit, sizeof(FogVolume), MAX_FOG_VOLUMES);

            volumetricUniformBuffer = Buffer::UniformBuffer(sizeof(VolumetricUniforms));
            resolveUniformBuffer = Buffer::UniformBuffer(sizeof(ResolveUniforms));

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

            CPUProfiler::Scope profilerScope("Render volumetric");
            GPUProfiler::Scope gpuScope("Render volumetric");

            auto lowResDepthTexture = target->GetData(target->GetVolumetricResolution())->depthTexture;
            auto depthTexture = target->GetData(FULL_RES)->depthTexture;

            auto fog = scene->renderState.fog;
            bool fogEnabled = fog && fog->enable;

            commandList->ImageMemoryBarrier(target->volumetricTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            if (fogEnabled)
                RenderFroxels(target, scene, commandList);

            Graphics::ImageBarrier imageBarriers[] = {
                {target->volumetricTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, {});

            {
                GPUProfiler::Scope sectionScope("Resolve");

                const int32_t groupSize = 8;

                auto res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

                auto groupCount = res / groupSize;
                groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
                groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

                auto clouds = scene->renderState.sky.clouds;
                auto cloudsEnabled = clouds && clouds->enable && scene->renderState.mainLight;

                resolvePipelineConfig.ManageMacro("CLOUDS", cloudsEnabled);
                resolvePipelineConfig.ManageMacro("FOG", fogEnabled);

                auto resolvePipeline = PipelineManager::GetPipeline(resolvePipelineConfig);
                commandList->BindPipeline(resolvePipeline);

                commandList->BindImage(target->lightingTexture.image, 3, 0);
                commandList->BindImage(target->volumetricTexture.image, target->volumetricTexture.sampler, 3, 1);
                commandList->BindImage(lowResDepthTexture->image, lowResDepthTexture->sampler, 3, 2);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 4);

                ResolveUniforms uniforms;

                uniforms.cloudsEnabled = cloudsEnabled ? 1 : 0;
                uniforms.downsampled2x = target->GetVolumetricResolution() == RenderResolution::HALF_RES ? 1 : 0;

                if (cloudsEnabled) {
                    uniforms.innerCloudRadius = scene->renderState.sky.planetRadius + clouds->baseAltitude;
                    uniforms.planetRadius = scene->renderState.sky.planetRadius;
                    uniforms.cloudDistanceLimit = clouds->distanceLimit;
                    uniforms.planetCenter = vec4(scene->renderState.sky.planetCenter, 1.0f);
                    commandList->BindImage(target->volumetricCloudsTexture.image, target->volumetricCloudsTexture.sampler, 3, 3);
                }

                resolveUniformBuffer.SetData(&uniforms, 0);
                commandList->BindBuffer(resolveUniformBuffer.Get(), 3, 5);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);
            }

            commandList->ImageMemoryBarrier(target->lightingTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

        }

        void VolumetricRenderer::RenderFroxels(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
            Graphics::CommandList* commandList) {

            GPUProfiler::Scope froxelScope("Froxels");

            auto renderState = &scene->renderState;
            auto fog = renderState->fog;
            auto& camera = renderState->camera;
            auto& mainLight = renderState->mainLight;

            auto resolution = ivec2(target->volumetricTexture.width, target->volumetricTexture.height);

            renderState->volumetricLightBuffer.Bind(commandList, 3, 8);
            renderState->volumetricShadowBuffer.Bind(commandList, 3, 9);

            VolumetricUniforms uniforms;
            uniforms.intensity = fog->volumetricIntensity;
            uniforms.directionalLightCount = 0;

            for (const auto& light : renderState->volumetricLights) {
                auto packedType = reinterpret_cast<const uint32_t&>(light.color.a);
                auto type = static_cast<LightType>(packedType);

                if (type != LightType::DirectionalLight)
                    break;

                uniforms.directionalLightCount++;
            }

            auto ocean = renderState->ocean;
            bool oceanEnabled = ocean && ocean->enable;

            if (oceanEnabled) {
                target->oceanDepthTexture.Bind(commandList, 3, 3);
                // Needs barrier
                target->oceanStencilTexture.Bind(commandList, 3, 4);
            }

            auto& fogUniform = uniforms.fog;
            fogUniform.extinctionCoefficient = fog->extinctionCoefficients;
            fogUniform.scatteringFactor = fog->scatteringFactor;
            fogUniform.extinctionFactor = fog->extinctionFactor;
            fogUniform.density = fog->density;
            fogUniform.heightFalloff = fog->heightFalloff;
            fogUniform.height = fog->height;
            fogUniform.ambientFactor = fog->ambientFactor;
            fogUniform.scatteringAnisotropy = glm::clamp(fog->scatteringAnisotropy, -0.999f, 0.999f);

            auto clouds = renderState->sky.clouds;
            bool cloudsEnabled = clouds && clouds->enable && mainLight;
            bool cloudShadowsEnabled = cloudsEnabled && clouds->castShadow;

            if (cloudsEnabled) {
                target->volumetricCloudsTexture.Bind(commandList, 3, 5);

                float cloudInnerRadius = renderState->sky.planetRadius + clouds->baseAltitude;
                uniforms.planetCenterAndRadius = vec4(renderState->sky.planetCenter, cloudInnerRadius);
            }

            if (cloudShadowsEnabled) {
                auto& cloudShadowUniform = uniforms.cloudShadow;

                clouds->shadowTexture.Bind(commandList, 3, 2);

                clouds->GetShadowMatrices(camera, normalize(mainLight->transformedProperties.directional.direction),
                    cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera.invViewMatrix;

                cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);
            }

            commandList->BindSampler(shadowSampler, 3, 6);
            commandList->BindSampler(shadowDepthSampler, 3, 12);

            volumetricUniformBuffer.SetData(&uniforms, 0);
            commandList->BindBuffer(volumetricUniformBuffer.Get(), 3, 7);

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

            auto totalClusterCount = clusterCount.x * clusterCount.y * clusterCount.z;

            if (lightCullingBuffer.GetElementCount() < totalClusterCount * 64) {
                lightCullingBuffer.SetSize(totalClusterCount * 64);
            }
            lightCullingBuffer.Bind(commandList, 3, 10);

            auto fogVolumeCount = int32_t(renderState->fogVolumes.size());
            bool fogVolumesEnabled = fogVolumeCount > 0;

            if (fogVolumesEnabled) {
                if (fogVolumeMaskBuffer.GetElementCount() < totalClusterCount) {
                    fogVolumeMaskBuffer.SetSize(totalClusterCount);
                }
                fogVolumeBuffer.SetData(renderState->fogVolumes.data(), 0, renderState->fogVolumes.size());

                fogVolumeBuffer.Bind(commandList, 3, 11);
                fogVolumeMaskBuffer.Bind(commandList, 3, 13);
            }

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
                .lightCount = std::min(4096, int32_t(renderState->volumetricLights.size())),
                .resetHistory = resetHistory ? 1 : 0,
                .fogVolumeCount = fogVolumeCount,
                .jitter = jitter,
                .extendedFar = camera.farPlane,
                .distributionExponent = glm::clamp(fog->froxelDistributionExponent, 0.1f, 4.0f)
            };

            auto lowResDepthTexture = target->GetData(target->GetVolumetricResolution())->depthTexture;

            {
                GPUProfiler::BeginQuery("Light culling");

                froxelCullingPipelineConfig.ManageMacro("FOG_VOLUMES", fogVolumesEnabled);
                auto pipeline = PipelineManager::GetPipeline(froxelCullingPipelineConfig);
                commandList->BindPipeline(pipeline);
                commandList->PushConstants("constants", &constants);

                commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);
                if (fogVolumesEnabled)
                    commandList->BufferMemoryBarrier(fogVolumeMaskBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT);
                commandList->Dispatch(clusterCount.x, clusterCount.y, clusterCount.z);
                commandList->BufferMemoryBarrier(lightCullingBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);
                if (fogVolumesEnabled)
                    commandList->BufferMemoryBarrier(fogVolumeMaskBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);
            }

            {
                GPUProfiler::EndAndBeginQuery("Inject");

                froxelInjectPipelineConfig.ManageMacro("CLOUD_SHADOWS", cloudShadowsEnabled);
                froxelInjectPipelineConfig.ManageMacro("LOCAL_LIGHTS", fog->localLights);
                froxelInjectPipelineConfig.ManageMacro("FOG_VOLUMES", fogVolumesEnabled);
                froxelInjectPipelineConfig.ManageMacro("SKY_PROBE", renderState->sky.GetProbe() != nullptr);
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
                GPUProfiler::EndAndBeginQuery("Filter");

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
                GPUProfiler::EndAndBeginQuery("Integrate");

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
                GPUProfiler::EndAndBeginQuery("Apply");

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

            GPUProfiler::EndQuery();

            std::swap(target->froxelTexture, target->historyFroxelTexture);

        }

    }

}

