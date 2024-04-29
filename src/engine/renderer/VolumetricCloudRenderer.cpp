#include "VolumetricCloudRenderer.h"

#include "common/RandomHelper.h"
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

            volumetricUniformBuffer = Buffer::UniformBuffer(sizeof(VolumetricCloudUniforms), 1);
            shadowUniformBuffer = Buffer::UniformBuffer(sizeof(CloudShadowUniforms), 1);
            shadowVolumetricUniformBuffer = Buffer::UniformBuffer(sizeof(VolumetricCloudUniforms), 1);

        }

        void VolumetricCloudRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            frameCount++;

            auto clouds = scene->sky.clouds;

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!clouds || !clouds->enable || !mainLightEntity.IsValid()) return;

            Graphics::Profiler::BeginQuery("Volumetric clouds");

            if (clouds->needsNoiseUpdate) {
                GenerateTextures(scene, commandList);
                clouds->needsNoiseUpdate = false;
            }

            auto downsampledRT = target->GetData(target->GetVolumetricResolution());
            auto downsampledHistoryRT = target->GetHistoryData(target->GetReflectionResolution());

            ivec2 res = ivec2(target->volumetricCloudsTexture.width, target->volumetricCloudsTexture.height);

            auto depthTexture = downsampledRT->depthTexture;
            auto velocityTexture = downsampledRT->velocityTexture;

            auto historyDepthTexture = downsampledHistoryRT->depthTexture;

            std::vector<Graphics::BufferBarrier> bufferBarriers;
            std::vector<Graphics::ImageBarrier> imageBarriers;
            
            {
                Graphics::Profiler::BeginQuery("Integrate");

                ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

                auto uniforms = GetUniformStructure(scene, mainLightEntity);
                volumetricUniformBuffer.SetData(&uniforms, 0);

                integratePipelineConfig.ManageMacro("STOCHASTIC_OCCLUSION_SAMPLING", clouds->stochasticOcclusionSampling);

                auto pipeline = PipelineManager::GetPipeline(integratePipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->ImageMemoryBarrier(target->swapVolumetricCloudsTexture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

                commandList->BindImage(target->swapVolumetricCloudsTexture.image, 3, 0);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 1);
                commandList->BindImage(clouds->shapeTexture.image, clouds->shapeTexture.sampler, 3, 2);
                commandList->BindImage(clouds->detailTexture.image, clouds->detailTexture.sampler, 3, 3);
                commandList->BindImage(clouds->coverageTexture.image, clouds->coverageTexture.sampler, 3, 4);
                commandList->BindImage(scramblingRankingTexture.image, scramblingRankingTexture.sampler, 3, 5);
                commandList->BindImage(sobolSequenceTexture.image, sobolSequenceTexture.sampler, 3, 6);

                volumetricUniformBuffer.Bind(commandList, 3, 7);

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

                imageBarriers = {
                    {target->swapVolumetricCloudsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->volumetricCloudsTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
                };

                commandList->PipelineBarrier(imageBarriers, bufferBarriers);

                commandList->BindImage(target->volumetricCloudsTexture.image, 3, 0);
                commandList->BindImage(target->swapVolumetricCloudsTexture.image, target->swapVolumetricCloudsTexture.sampler, 3, 1);
                commandList->BindImage(velocityTexture->image, velocityTexture->sampler, 3, 2);
                commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 3);
                commandList->BindImage(target->historyVolumetricCloudsTexture.image, target->historyVolumetricCloudsTexture.sampler, 3, 4);
                commandList->BindImage(historyDepthTexture->image, historyDepthTexture->sampler, 3, 5);

                int32_t resetHistory = !target->HasHistory() ? 1 : 0;
                commandList->PushConstants("constants", &resetHistory, sizeof(int32_t));

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
            }

            {
                Graphics::Profiler::BeginQuery("Copy to history");

                // Need barriers for both images
                imageBarriers = {
                    {target->volumetricCloudsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {target->historyVolumetricCloudsTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                commandList->CopyImage(target->volumetricCloudsTexture.image, target->historyVolumetricCloudsTexture.image);

                imageBarriers = {
                    {target->volumetricCloudsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {target->historyVolumetricCloudsTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
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
            shadowUniforms.shadowSampleFraction = clouds->shadowSampleFraction;
            shadowUniformBuffer.SetData(&shadowUniforms, 0);

            auto uniforms = GetUniformStructure(scene, mainLightEntity);
            uniforms.distanceLimit = 10e9f;
            shadowVolumetricUniformBuffer.SetData(&uniforms, 0);

            shadowVolumetricUniformBuffer.Bind(commandList, 3, 7);
            shadowUniformBuffer.Bind(commandList, 3, 8);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(clouds->shadowTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void VolumetricCloudRenderer::GenerateTextures(Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto clouds = scene->sky.clouds;
            if (!clouds) return;

            GenerateShapeTexture(commandList, &clouds->shapeTexture, clouds->shapeScale);
            GenerateDetailTexture(commandList, &clouds->detailTexture, clouds->detailScale);

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
            Texture::Texture3D* texture, float baseScale) {

            Graphics::Profiler::BeginQuery("Generate shape cloud texture");

            // Expect the resolution to be a power of 2 and larger equal 4
            ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

            auto pipeline = PipelineManager::GetPipeline(shapeNoisePipelineConfig);
            commandList->BindPipeline(pipeline);

            auto randomFloat = Common::Random::SampleUniformFloat() * 10.0f;
            commandList->PushConstants("constants", &randomFloat, sizeof(float));

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(texture->image, 3, 0);
            
            commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void VolumetricCloudRenderer::GenerateDetailTexture(Graphics::CommandList* commandList,
            Texture::Texture3D* texture, float baseScale) {

            Graphics::Profiler::BeginQuery("Generate detail cloud texture");

            // Expect the resolution to be a power of 2 and larger equal 4
            ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

            auto pipeline = PipelineManager::GetPipeline(detailNoisePipelineConfig);
            commandList->BindPipeline(pipeline);

            auto randomFloat = Common::Random::SampleUniformFloat() * 10.0f;
            commandList->PushConstants("constants", &randomFloat, sizeof(float));

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(texture->image, 3, 0);

            commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        VolumetricCloudRenderer::VolumetricCloudUniforms VolumetricCloudRenderer::GetUniformStructure(
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

            float innerRadius = scene->sky.planetRadius + clouds->minHeight;
            float outerRadius = scene->sky.planetRadius + clouds->maxHeight;
            vec3 cloudCenter = scene->sky.planetCenter;

            VolumetricCloudUniforms uniforms{
                .planetRadius = scene->sky.planetRadius,
                .innerRadius = innerRadius,
                .outerRadius = outerRadius,
                .distanceLimit = clouds->distanceLimit,

                .heightStretch = clouds->heightStretch,

                .coverageScale = clouds->coverageScale,
                .shapeScale = clouds->shapeScale,
                .detailScale = clouds->detailScale,
                .coverageSpeed = clouds->coverageSpeed,
                .shapeSpeed = clouds->shapeSpeed,
                .detailSpeed = clouds->detailSpeed,
                .detailStrength = clouds->detailStrength,

                .extinctionFactor = clouds->scattering.extinctionFactor,
                .scatteringFactor = clouds->scattering.scatteringFactor,

                .eccentricityFirstPhase = clouds->scattering.eccentricityFirstPhase,
                .eccentricitySecondPhase = clouds->scattering.eccentricitySecondPhase,
                .phaseAlpha = clouds->scattering.phaseAlpha,

                .densityMultiplier = clouds->densityMultiplier,

                .time = Clock::Get(),
                .frameSeed = frameCount,

                .sampleCount = clouds->sampleCount,
                .occlusionSampleCount = clouds->occlusionSampleCount,

                .darkEdgeDirect = clouds->darkEdgeFocus,
                .darkEdgeDetail = clouds->darkEdgeAmbient,

                .extinctionCoefficients = clouds->scattering.extinctionCoefficients,
                .planetCenter = vec4(cloudCenter, 1.0f)
            };

            if (mainLightEntity.IsValid()) {
                auto& light = mainLightEntity.GetComponent<LightComponent>();
                uniforms.light.direction = vec4(light.transformedProperties.directional.direction, 0.0f);
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