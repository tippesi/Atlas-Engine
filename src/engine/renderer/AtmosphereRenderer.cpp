#include "AtmosphereRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

        void AtmosphereRenderer::Init(Graphics::GraphicsDevice *device) {

            defaultPipelineConfig = PipelineConfig("atmosphere.csh");
            cubeMapPipelineConfig = PipelineConfig("atmosphere.csh", {"ENVIRONMENT_PROBE"});

            // 2 elements of size for default rendering plus one cube map
            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms), 2);
            probeMatricesBuffer = Buffer::UniformBuffer(sizeof(mat4) * 6);

		}

		void AtmosphereRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

			auto sun = scene->sky.sun;
			auto atmosphere = scene->sky.atmosphere;
			if (!sun || !atmosphere) return;

            Graphics::Profiler::BeginQuery("Atmosphere");

            auto pipeline = PipelineManager::GetPipeline(defaultPipelineConfig);
            commandList->BindPipeline(pipeline);

			auto location = camera->GetLocation();

            auto rtData = target->GetData(FULL_RES);
            auto velocityTexture = rtData->velocityTexture;
            auto depthTexture = rtData->depthTexture;

            Uniforms uniforms {
                .ivMatrix = camera->invViewMatrix,
                .ipMatrix = camera->invProjectionMatrix,
                .cameraLocation = vec4(location, 1.0f),
                .planetCenter = vec4(scene->sky.planetCenter, 1.0f),
                .sunDirection = vec4(sun->direction, 0.0f),
                .sunIntensity = sun->intensity,
                .planetRadius = scene->sky.planetRadius,
                .atmosphereRadius = scene->sky.planetRadius + atmosphere->height
            };
            uniformBuffer.SetData(&uniforms, 0, 1);

            std::vector<Graphics::BufferBarrier> bufferBarriers;
            std::vector<Graphics::ImageBarrier> imageBarriers = {
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {velocityTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindImage(velocityTexture->image, 3, 1);

            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 2);
            commandList->BindBufferOffset(uniformBuffer.Get(), uniformBuffer.GetAlignedOffset(0), 3, 3);

            auto resolution = ivec2(target->GetWidth(), target->GetHeight());
            auto groupCount = resolution / 8;

            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            imageBarriers = {
                {target->lightingTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {velocityTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            };
            commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

			Graphics::Profiler::EndQuery();

		}

		void AtmosphereRenderer::Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene,
            Graphics::CommandList* commandList) {

            auto sun = scene->sky.sun;
			auto atmosphere = scene->sky.atmosphere;
			if (!sun || !atmosphere) return;

			Graphics::Profiler::BeginQuery("Atmosphere environment probe");

            auto pipeline = PipelineManager::GetPipeline(cubeMapPipelineConfig);
            commandList->BindPipeline(pipeline);

			commandList->ImageMemoryBarrier(probe->cubemap.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            std::vector<mat4> matrices = probe->viewMatrices;
            for (auto& matrix : matrices) matrix = glm::inverse(matrix);

            Uniforms uniforms {
                .ipMatrix = glm::inverse(probe->projectionMatrix),
                .cameraLocation = vec4(probe->GetPosition(), 1.0f),
                .planetCenter = vec4(scene->sky.planetCenter, 1.0f),
                .sunDirection = vec4(sun->direction, 0.0f),
                .sunIntensity = sun->intensity,
                .planetRadius = scene->sky.planetRadius,
                .atmosphereRadius = scene->sky.planetRadius + atmosphere->height
            };
            uniformBuffer.SetData(&uniforms, 1, 1);
            probeMatricesBuffer.SetData(matrices.data(), 0, 1);

            commandList->BindImage(probe->cubemap.image, 3, 0);
            commandList->BindBufferOffset(uniformBuffer.Get(), uniformBuffer.GetAlignedOffset(1), 3, 3);
            commandList->BindBuffer(probeMatricesBuffer.Get(), 3, 4);

			Graphics::Profiler::BeginQuery("Render probe faces");

            auto resolution = ivec2(probe->resolution);
            auto groupCount = resolution / 8;

            groupCount.x += ((groupCount.x * 8 == resolution.x) ? 0 : 1);
            groupCount.y += ((groupCount.y * 8 == resolution.y) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 6);

            Graphics::Profiler::EndAndBeginQuery("Generate mipmaps");

            commandList->ImageMemoryBarrier(probe->cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            // The cube map generating automatically transforms the image layout to read-only optimal
            commandList->GenerateMipMap(probe->cubemap.image);

			probe->cubemap.GenerateMipmap();

			Graphics::Profiler::EndQuery();
			Graphics::Profiler::EndQuery();

		}


	}

}