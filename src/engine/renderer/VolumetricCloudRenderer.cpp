#include "VolumetricCloudRenderer.h"

#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        void VolumetricCloudRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("scrambling_ranking.png", false, 4);
            scramblingRankingTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            scramblingRankingTexture.SetData(noiseImage->GetData());

            noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("sobol.png");
            sobolSequenceTexture = Texture::Texture2D(noiseImage->width, noiseImage->height,
                VK_FORMAT_R8G8B8A8_UNORM);
            sobolSequenceTexture.SetData(noiseImage->GetData());

            shapeNoisePipelineConfig = PipelineConfig("clouds/shapeNoise.csh");
            detailNoisePipelineConfig = PipelineConfig("clouds/detailNoise.csh");
            integratePipelineConfig = PipelineConfig("clouds/integrate.csh");
            temporalPipelineConfig = PipelineConfig("clouds/temporal.csh");
            shadowPipelineConfig = PipelineConfig("clouds/shadow.csh");

            volumetricUniformBuffer = Buffer::UniformBuffer(sizeof(CloudUniforms), 1);
            shadowUniformBuffer = Buffer::UniformBuffer(sizeof(CloudShadowUniforms), 1);
            shadowVolumetricUniformBuffer = Buffer::UniformBuffer(sizeof(CloudUniforms), 1);

        }

        void VolumetricCloudRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            frameCount++;

            auto clouds = scene->sky.clouds;

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!clouds || !clouds->enable || !mainLightEntity.IsValid()) return;

            // Resolution settings are shared with the volumetric fog
            bool halfResolution = clouds->quality < 2;
            if (halfResolution && target->GetVolumetricResolution() == FULL_RES)
                target->SetVolumetricResolution(HALF_RES);
            else if (!halfResolution && target->GetVolumetricResolution() != FULL_RES)
                target->SetVolumetricResolution(FULL_RES);

            Graphics::Profiler::BeginQuery("Volumetric clouds");

            if (clouds->needsNoiseUpdate) {
                GenerateTextures(scene, commandList);
                clouds->needsNoiseUpdate = false;
            }

            if (target->historyVolumetricCloudsTexture.image->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                commandList->ImageMemoryBarrier(target->historyVolumetricCloudsTexture.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
            }

            auto downsampledRT = target->GetData(target->GetVolumetricResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetVolumetricResolution());

            ivec2 res = ivec2(target->volumetricCloudsTexture.width, target->volumetricCloudsTexture.height);

            auto depthTexture = downsampledRT->depthTexture;
            auto velocityTexture = downsampledRT->velocityTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;
            
            {
                Graphics::Profiler::BeginQuery("Integrate");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                auto uniforms = GetUniformStructure(scene, mainLightEntity);
                volumetricUniformBuffer.SetData(&uniforms, 0);

                auto ocean = scene->ocean;
                bool oceanEnabled = ocean && ocean->enable;

                integratePipelineConfig.ManageMacro("OCEAN", oceanEnabled);

                auto pipeline = PipelineManager::GetPipeline(integratePipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->ImageMemoryBarrier(target->swapVolumetricCloudsTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                commandList->BindImage(target->swapVolumetricCloudsTexture.image, 3, 0);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 1);
                commandList->BindImage(clouds->shapeTexture.image, clouds->shapeTexture.sampler, 3, 2);
                commandList->BindImage(clouds->detailTexture.image, clouds->detailTexture.sampler, 3, 3);
                commandList->BindImage(clouds->coverageTexture.image, clouds->coverageTexture.sampler, 3, 4);
                volumetricUniformBuffer.Bind(commandList, 3, 6);

                commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 7);
                commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 8);               

                if (oceanEnabled) {
                    target->oceanDepthTexture.Bind(commandList, 3, 9);
                    target->oceanStencilTexture.Bind(commandList, 3, 10);
                }                

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
            }

            {
                Graphics::Profiler::BeginQuery("Temporal accumulation");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                auto pipeline = PipelineManager::GetPipeline(temporalPipelineConfig);
                commandList->BindPipeline(pipeline);

                Graphics::ImageBarrier imageBarriers[] = {
                    {target->swapVolumetricCloudsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->volumetricCloudsTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                };
                commandList->PipelineBarrier(imageBarriers, {});

                commandList->BindImage(target->volumetricCloudsTexture.image, 3, 0);
                commandList->BindImage(target->swapVolumetricCloudsTexture.image, target->swapVolumetricCloudsTexture.sampler, 3, 1);
                commandList->BindImage(velocityTexture->image, velocityTexture->sampler, 3, 2);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 3);
                commandList->BindImage(target->historyVolumetricCloudsTexture.image, target->historyVolumetricCloudsTexture.sampler, 3, 4);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 5);

                CloudTemporalPushConstants constants {
                    .resetHistory = !target->HasHistory() ? 1 : 0,
                    .downsampled2x = target->GetVolumetricResolution() == HALF_RES ? 1 : 0
                };
                commandList->PushConstants("constants", &constants, sizeof(CloudTemporalPushConstants));

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
            }

            {
                Graphics::Profiler::BeginQuery("Copy to history");

                // Need barriers for both images
                Graphics::ImageBarrier preImageBarriers[] = {
                    {target->volumetricCloudsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {target->historyVolumetricCloudsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                };
                commandList->PipelineBarrier(preImageBarriers, {},
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                commandList->CopyImage(target->volumetricCloudsTexture.image, target->historyVolumetricCloudsTexture.image);

                Graphics::ImageBarrier postImageBarriers[] = {
                    {target->volumetricCloudsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyVolumetricCloudsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(postImageBarriers, {},
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

                Graphics::Profiler::EndQuery();
            }
            
            Graphics::Profiler::EndQuery();

        }

        void VolumetricCloudRenderer::RenderShadow(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto clouds = scene->sky.clouds;
            
            auto mainLightEntity = GetMainLightEntity(scene);
            if (!clouds || !clouds->enable || !clouds->castShadow || !mainLightEntity.IsValid()) return;

            auto& light = mainLightEntity.GetComponent<LightComponent>();

            Graphics::Profiler::BeginQuery("Volumetric cloud shadow");

            if (clouds->needsNoiseUpdate) {
                GenerateTextures(scene, commandList);
                clouds->needsNoiseUpdate = false;
            }

            auto& camera = scene->GetMainCamera();
            auto shadowMap = clouds->shadowTexture;

            auto res = ivec2(shadowMap.width, shadowMap.height);

            ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
            groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

            commandList->ImageMemoryBarrier(clouds->shadowTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            auto pipeline = PipelineManager::GetPipeline(shadowPipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->BindImage(clouds->shadowTexture.image, 3, 0);
            clouds->shapeTexture.Bind(commandList, 3, 2);
            clouds->detailTexture.Bind(commandList, 3, 3);
            clouds->coverageTexture.Bind(commandList, 3, 4);

            CloudShadowUniforms shadowUniforms;
            clouds->GetShadowMatrices(camera, glm::normalize(light.transformedProperties.directional.direction),
                shadowUniforms.ivMatrix, shadowUniforms.ipMatrix);

            shadowUniforms.ipMatrix = glm::inverse(shadowUniforms.ipMatrix);
            shadowUniforms.ivMatrix = glm::inverse(shadowUniforms.ivMatrix);
            shadowUniforms.lightDirection = vec4(normalize(light.transformedProperties.directional.direction), 0.0f);
            shadowUniforms.shadowSampleFraction = 4;
            shadowUniformBuffer.SetData(&shadowUniforms, 0);

            auto uniforms = GetUniformStructure(scene, mainLightEntity);
            uniforms.distanceLimit = 10e9f;
            shadowVolumetricUniformBuffer.SetData(&uniforms, 0);

            shadowVolumetricUniformBuffer.Bind(commandList, 3, 6);
            shadowUniformBuffer.Bind(commandList, 3, 9);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(clouds->shadowTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void VolumetricCloudRenderer::GenerateTextures(Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto clouds = scene->sky.clouds;
            if (!clouds) return;

            GenerateShapeTexture(commandList, &clouds->shapeTexture, clouds->variationSeed);
            GenerateDetailTexture(commandList, &clouds->detailTexture, clouds->variationSeed + 3.68f);

            commandList->ImageMemoryBarrier(clouds->shapeTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->ImageMemoryBarrier(clouds->detailTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->GenerateMipMaps(clouds->shapeTexture.image);
            commandList->GenerateMipMaps(clouds->detailTexture.image);

        }

        void VolumetricCloudRenderer::GenerateShapeTexture(Graphics::CommandList* commandList,
            Texture::Texture3D* texture, float seed) {

            Graphics::Profiler::BeginQuery("Generate shape cloud texture");

            // Expect the resolution to be a power of 2 and larger equal 4
            ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

            auto pipeline = PipelineManager::GetPipeline(shapeNoisePipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->PushConstants("constants", &seed, sizeof(float));

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(texture->image, 3, 0);
            
            commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void VolumetricCloudRenderer::GenerateDetailTexture(Graphics::CommandList* commandList,
            Texture::Texture3D* texture, float seed) {

            Graphics::Profiler::BeginQuery("Generate detail cloud texture");

            // Expect the resolution to be a power of 2 and larger equal 4
            ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

            auto pipeline = PipelineManager::GetPipeline(detailNoisePipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->PushConstants("constants", &seed, sizeof(float));

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(texture->image, 3, 0);

            commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        VolumetricCloudRenderer::CloudUniforms VolumetricCloudRenderer::GetUniformStructure(
            Ref<Scene::Scene> scene, Scene::Entity mainLightEntity) {

            auto clouds = scene->sky.clouds;

            /*
            auto& camera = scene->GetMainCamera();
            float cloudsInnerRadius = scene->sky.planetRadius + clouds->minHeight;
            float cloudsOuterRadius = scene->sky.planetRadius + clouds->maxHeight;

            // This is an option for the future to make the clouds wrap down at the distance limit into the horizon
            float radiusDiff = cloudsOuterRadius - cloudsInnerRadius;

            float outerRadius = 5.0f * clouds->distanceLimit;
            float innerRadius = 5.0f * clouds->distanceLimit - radiusDiff;

            float innerHeight = cloudsInnerRadius - scene->sky.planetRadius;

            vec3 planetCenterDir = normalize(camera.GetLocation() - scene->sky.planetCenter);
            vec3 surfacePos = scene->sky.planetCenter + planetCenterDir * scene->sky.planetRadius;

            vec3 cloudCenter = surfacePos - (innerRadius - innerHeight) * planetCenterDir;
            */        

            float innerRadius = scene->sky.planetRadius + clouds->baseAltitude;
            float outerRadius = innerRadius + clouds->layerThickness;
            vec3 cloudCenter = scene->sky.planetCenter;

            // Low, medium, high and ultra quality
            const int32_t sampleCounts[] = { 48, 96, 160, 256 };
            const int32_t occlusionSampleCounts[] = { 4, 8, 12, 16 };
            auto quality = glm::clamp(clouds->quality, 0, 3);

            auto windVelocity = glm::length(scene->wind.direction) > 0.0f ?
                glm::normalize(scene->wind.direction) * scene->wind.speed : vec2(0.0f);
            // The seed moves the weather map, such that a different seed gives different cloud placements
            auto coverageOffset = glm::fract(vec2(0.137f, 0.271f) * (clouds->variationSeed - 4.23f));

            CloudUniforms uniforms{
                .planetRadius = scene->sky.planetRadius,
                .innerRadius = innerRadius,
                .outerRadius = outerRadius,
                .distanceLimit = clouds->distanceLimit,

                .coverageScale = 120.0f / clouds->cloudSize,
                .shapeScale = 720.0f / clouds->cloudSize,
                .verticalShapeScale = glm::mix(2.2f, 0.9f, clouds->verticalDevelopment),
                .detailScale = glm::mix(6.0f, 13.8125f, clouds->detail),
                .detailStrength = clouds->detail,
                .densityMultiplier = clouds->densityMultiplier,

                .coverage = clouds->coverage,
                .cloudType = clouds->cloudType,
                .bottomType = glm::mix(0.35f, 0.10f, clouds->cloudType),
                .heightVariation = glm::mix(0.45f, 0.90f, clouds->verticalDevelopment),
                .anvilBias = 0.3f * clouds->cloudType * clouds->verticalDevelopment,
                .shapeErosion = glm::mix(0.35f, 0.65f, clouds->cloudType),

                .extinctionFactor = clouds->scattering.extinctionFactor,
                .scatteringFactor = clouds->scattering.scatteringFactor,
                .eccentricityFirstPhase = clouds->scattering.eccentricityFirstPhase,
                .eccentricitySecondPhase = clouds->scattering.eccentricitySecondPhase,
                .phaseAlpha = clouds->scattering.phaseAlpha,

                .time = Clock::Get(),
                .frameSeed = frameCount,

                .sampleCount = sampleCounts[quality],
                .occlusionSampleCount = occlusionSampleCounts[quality],

                .extinctionCoefficients = clouds->scattering.extinctionCoefficients,
                .planetCenter = vec4(cloudCenter, 1.0f),
                .windAndCoverageOffset = vec4(windVelocity, coverageOffset)
            };

            if (mainLightEntity.IsValid()) {
                auto& light = mainLightEntity.GetComponent<LightComponent>();
                uniforms.light.direction = vec4(normalize(light.transformedProperties.directional.direction), 0.0f);
                uniforms.light.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), 1.0f);
                uniforms.light.intensity = light.intensity;
            }
            else {
                uniforms.light.intensity = 0.0f;
            }

            return uniforms;

        }

    }

}
