#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"
#include "../common/ColorConverter.h"

namespace Atlas {

    namespace Renderer {

        void AtmosphereRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            defaultPipelineConfig = PipelineConfig("atmosphere/atmosphere.csh");
            cubeMapPipelineConfig = PipelineConfig("atmosphere/atmosphere.csh", { "ENVIRONMENT_PROBE" });
            transmittancePipelineConfig = PipelineConfig("atmosphere/transmittance.csh");
            multipleScatteringPipelineConfig = PipelineConfig("atmosphere/multipleScattering.csh");
            skyViewPipelineConfig = PipelineConfig("atmosphere/skyView.csh");

            uniformBuffer = Buffer::UniformBuffer(sizeof(RenderUniforms), 2);
            transmittanceUniformBuffer = Buffer::UniformBuffer(sizeof(AtmosphereParameters));
            multipleScatteringUniformBuffer = Buffer::UniformBuffer(sizeof(MultipleScatteringUniforms));
            skyViewUniformBuffer = Buffer::UniformBuffer(sizeof(SkyViewUniforms));
            probeMatricesBuffer = Buffer::UniformBuffer(sizeof(mat4) * 6);

        }

        void AtmosphereRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene,
            Graphics::CommandList* commandList) {

            auto atmosphere = scene->sky.atmosphere;

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!mainLightEntity.IsValid() || !atmosphere)
                return;

            auto& camera = scene->GetMainCamera();
            auto& light = mainLightEntity.GetComponent<LightComponent>();
            vec3 cameraLocation = camera.GetLocation();
            vec3 sunDirection = GetSunDirection(light);
            vec3 sunRadiance = GetSunRadiance(light);
            auto atmosphereParameters = GetAtmosphereParameters(*atmosphere, scene->sky);

            Graphics::Profiler::BeginQuery("Atmosphere");

            if (atmosphere->needsUpdate || HasMissingLuts(*atmosphere, true)) {
                MultipleScatteringUniforms multipleScatteringUniforms {
                    .sunRadiance = vec4(sunRadiance, 0.0f),
                    .atmosphere = atmosphereParameters
                };

                SkyViewUniforms skyViewUniforms {
                    .cameraLocation = vec4(cameraLocation, 1.0f),
                    .planetCenter = vec4(scene->sky.planetCenter, 1.0f),
                    .sunDirection = vec4(sunDirection, 0.0f),
                    .sunRadiance = vec4(sunRadiance, 0.0f),
                    .atmosphere = atmosphereParameters
                };

                UpdateTransmittanceLut(atmosphereParameters, atmosphere.get(), commandList);
                UpdateMultipleScatteringLut(multipleScatteringUniforms, atmosphere.get(), commandList);
                UpdateSkyViewLut(skyViewUniforms, atmosphere.get(), commandList);
                atmosphere->needsUpdate = false;
            }

            auto pipeline = PipelineManager::GetPipeline(defaultPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rtData = target->GetData(FULL_RES);
            auto velocityTexture = rtData->velocityTexture;
            auto depthTexture = rtData->depthTexture;

            RenderUniforms uniforms {
                .ivMatrix = camera.invViewMatrix,
                .ipMatrix = camera.invProjectionMatrix,
                .cameraLocation = vec4(cameraLocation, 1.0f),
                .planetCenter = vec4(scene->sky.planetCenter, 1.0f),
                .sunDirection = vec4(sunDirection, 0.0f),
                .sunRadiance = vec4(sunRadiance, 0.0f),
                .atmosphere = atmosphereParameters
            };
            uniformBuffer.SetData(&uniforms, 0);

            Graphics::ImageBarrier preImageBarriers[] = {
                { target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT },
                { velocityTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT },
            };
            commandList->PipelineBarrier(preImageBarriers, {});

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(velocityTexture->image, 3, 1);
            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindBufferOffset(uniformBuffer.Get(), uniformBuffer.GetAlignedOffset(0), 3, 3);
            commandList->BindImage(atmosphere->skyViewLutTexture.image,
                atmosphere->skyViewLutTexture.sampler, 3, 4);
            commandList->BindImage(atmosphere->transmittanceLutTexture.image,
                atmosphere->transmittanceLutTexture.sampler, 3, 5);

            auto resolution = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            auto groupCount = GetGroupCount(resolution);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::ImageBarrier postImageBarriers[] = {
                { target->lightingTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT },
                { velocityTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT },
            };
            commandList->PipelineBarrier(postImageBarriers, {}, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            Graphics::Profiler::EndQuery();

        }

        void AtmosphereRenderer::Render(Ref<Lighting::EnvironmentProbe> probe, Ref<Scene::Scene> scene,
            Graphics::CommandList* commandList) {

            auto atmosphere = scene->sky.atmosphere;

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!mainLightEntity.IsValid() || !atmosphere) {
                commandList->ImageMemoryBarrier(probe->generatedCubemap.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                return;
            }

            auto& light = mainLightEntity.GetComponent<LightComponent>();
            vec3 sunDirection = GetSunDirection(light);
            vec3 sunRadiance = GetSunRadiance(light);
            auto atmosphereParameters = GetAtmosphereParameters(*atmosphere, scene->sky);

            MultipleScatteringUniforms multipleScatteringUniforms {
                .sunRadiance = vec4(sunRadiance, 0.0f),
                .atmosphere = atmosphereParameters
            };

            Graphics::Profiler::BeginQuery("Atmosphere environment probe");

            if (atmosphere->needsUpdate || HasMissingLuts(*atmosphere, false)) {
                UpdateTransmittanceLut(atmosphereParameters, atmosphere.get(), commandList);
                UpdateMultipleScatteringLut(multipleScatteringUniforms, atmosphere.get(), commandList);
            }

            auto pipeline = PipelineManager::GetPipeline(cubeMapPipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->ImageMemoryBarrier(probe->generatedCubemap.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            std::vector<mat4> matrices = probe->viewMatrices;
            for (auto& matrix : matrices)
                matrix = glm::inverse(matrix);

            RenderUniforms uniforms {
                .ipMatrix = glm::inverse(probe->projectionMatrix),
                .cameraLocation = vec4(probe->GetPosition(), 1.0f),
                .planetCenter = vec4(scene->sky.planetCenter, 1.0f),
                .sunDirection = vec4(sunDirection, 0.0f),
                .sunRadiance = vec4(sunRadiance, 0.0f),
                .atmosphere = atmosphereParameters
            };
            uniformBuffer.SetData(&uniforms, 1);
            probeMatricesBuffer.SetData(matrices.data(), 0);

            commandList->BindImage(probe->generatedCubemap.image, 3, 0);
            commandList->BindBufferOffset(uniformBuffer.Get(), uniformBuffer.GetAlignedOffset(1), 3, 3);
            commandList->BindImage(atmosphere->transmittanceLutTexture.image,
                atmosphere->transmittanceLutTexture.sampler, 3, 4);
            commandList->BindImage(atmosphere->multipleScatteringLutTexture.image,
                atmosphere->multipleScatteringLutTexture.sampler, 3, 5);
            commandList->BindBufferOffset(probeMatricesBuffer.Get(), 0, 3, 6);

            Graphics::Profiler::BeginQuery("Render probe faces");

            auto resolution = ivec2(probe->GetCubemap().width, probe->GetCubemap().height);
            auto groupCount = GetGroupCount(resolution);

            commandList->Dispatch(groupCount.x, groupCount.y, 6);

            Graphics::Profiler::EndAndBeginQuery("Generate mipmaps");

            commandList->ImageMemoryBarrier(probe->generatedCubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            commandList->GenerateMipMaps(probe->generatedCubemap.image);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

        }

        void AtmosphereRenderer::UpdateTransmittanceLut(AtmosphereParameters atmosphereParameters,
            Lighting::Atmosphere* atmosphere, Graphics::CommandList* commandList) {

            EnsureLutTexture(atmosphere->transmittanceLutTexture, transmittanceLutResolution);

            Graphics::Profiler::BeginQuery("Atmosphere transmittance");

            auto pipeline = PipelineManager::GetPipeline(transmittancePipelineConfig);
            commandList->BindPipeline(pipeline);

            transmittanceUniformBuffer.SetData(&atmosphereParameters, 0);

            commandList->ImageMemoryBarrier(atmosphere->transmittanceLutTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);
            commandList->BindImage(atmosphere->transmittanceLutTexture.image, 3, 0);
            commandList->BindBufferOffset(transmittanceUniformBuffer.Get(),
                transmittanceUniformBuffer.GetAlignedOffset(0), 3, 1);

            auto resolution = ivec2(atmosphere->transmittanceLutTexture.width,
                atmosphere->transmittanceLutTexture.height);
            auto groupCount = GetGroupCount(resolution);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(atmosphere->transmittanceLutTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            Graphics::Profiler::EndQuery();

        }

        void AtmosphereRenderer::UpdateMultipleScatteringLut(
            const MultipleScatteringUniforms& uniforms, Lighting::Atmosphere* atmosphere,
            Graphics::CommandList* commandList) {

            EnsureLutTexture(atmosphere->multipleScatteringLutTexture, multipleScatteringLutResolution);

            Graphics::Profiler::BeginQuery("Atmosphere multiple scattering");

            auto pipeline = PipelineManager::GetPipeline(multipleScatteringPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto uploadUniforms = uniforms;
            multipleScatteringUniformBuffer.SetData(&uploadUniforms, 0);

            commandList->ImageMemoryBarrier(atmosphere->multipleScatteringLutTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);
            commandList->BindImage(atmosphere->multipleScatteringLutTexture.image, 3, 0);
            commandList->BindImage(atmosphere->transmittanceLutTexture.image,
                atmosphere->transmittanceLutTexture.sampler, 3, 1);
            commandList->BindBufferOffset(multipleScatteringUniformBuffer.Get(),
                multipleScatteringUniformBuffer.GetAlignedOffset(0), 3, 2);

            auto resolution = ivec2(atmosphere->multipleScatteringLutTexture.width,
                atmosphere->multipleScatteringLutTexture.height);
            auto groupCount = GetGroupCount(resolution);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(atmosphere->multipleScatteringLutTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            Graphics::Profiler::EndQuery();

        }

        void AtmosphereRenderer::UpdateSkyViewLut(const SkyViewUniforms& uniforms,
            Lighting::Atmosphere* atmosphere, Graphics::CommandList* commandList) {

            EnsureLutTexture(atmosphere->skyViewLutTexture, skyViewLutResolution);

            Graphics::Profiler::BeginQuery("Atmosphere sky view");

            auto pipeline = PipelineManager::GetPipeline(skyViewPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto uploadUniforms = uniforms;
            skyViewUniformBuffer.SetData(&uploadUniforms, 0);

            commandList->ImageMemoryBarrier(atmosphere->skyViewLutTexture.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);
            commandList->BindImage(atmosphere->skyViewLutTexture.image, 3, 0);
            commandList->BindImage(atmosphere->transmittanceLutTexture.image,
                atmosphere->transmittanceLutTexture.sampler, 3, 1);
            commandList->BindImage(atmosphere->multipleScatteringLutTexture.image,
                atmosphere->multipleScatteringLutTexture.sampler, 3, 2);
            commandList->BindBufferOffset(skyViewUniformBuffer.Get(),
                skyViewUniformBuffer.GetAlignedOffset(0), 3, 3);

            auto resolution = ivec2(atmosphere->skyViewLutTexture.width,
                atmosphere->skyViewLutTexture.height);
            auto groupCount = GetGroupCount(resolution);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(atmosphere->skyViewLutTexture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            Graphics::Profiler::EndQuery();

        }

        AtmosphereRenderer::AtmosphereParameters AtmosphereRenderer::GetAtmosphereParameters(
            const Lighting::Atmosphere& atmosphere, const Lighting::Sky& sky) {

            return {
                .rayleighScatteringCoeff = vec4(atmosphere.rayleighScatteringCoeff, 0.0f),
                .groundAlbedo = vec4(atmosphere.groundAlbedo, 0.0f),
                .mieScatteringCoeff = atmosphere.mieScatteringCoeff,
                .rayleighHeightScale = atmosphere.rayleighHeightScale,
                .mieHeightScale = atmosphere.mieHeightScale,
                .surfaceHeightOffset = -(sky.planetCenter.y + sky.planetRadius),
                .planetRadius = sky.planetRadius,
                .atmosphereRadius = sky.planetRadius + atmosphere.height
            };

        }

        vec3 AtmosphereRenderer::GetSunDirection(const LightComponent& light) {

            return normalize(-light.transformedProperties.directional.direction);

        }

        vec3 AtmosphereRenderer::GetSunRadiance(const LightComponent& light) {

            return Common::ColorConverter::ConvertSRGBToLinear(light.color) * light.intensity;

        }

        ivec2 AtmosphereRenderer::GetGroupCount(ivec2 resolution) {

            auto groupCount = resolution / groupSize;

            groupCount.x += ((groupCount.x * groupSize == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * groupSize == resolution.y) ? 0 : 1);

            return groupCount;

        }

        bool AtmosphereRenderer::HasMissingLuts(const Lighting::Atmosphere& atmosphere,
            bool includeSkyView) {

            return !atmosphere.transmittanceLutTexture.IsValid() ||
                !atmosphere.multipleScatteringLutTexture.IsValid() ||
                (includeSkyView && !atmosphere.skyViewLutTexture.IsValid());

        }

        void AtmosphereRenderer::EnsureLutTexture(Texture::Texture2D& texture, ivec2 resolution) {

            if (texture.IsValid())
                return;

            texture = Texture::Texture2D(resolution.x, resolution.y,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge,
                Texture::Filtering::Linear);

        }

    }

}
