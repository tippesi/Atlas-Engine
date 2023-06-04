#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		void OceanRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

			Helper::GeometryHelper::GenerateGridVertexArray(vertexArray, 129, 1.0f / 128.0f);

            causticPipelineConfig = PipelineConfig("ocean/caustics.csh");

            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));
            lightUniformBuffer = Buffer::UniformBuffer(sizeof(Light));

		}

		void OceanRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

			if (!scene->ocean || !scene->ocean->enable)
				return;

			Graphics::Profiler::BeginQuery("Ocean");

			auto ocean = scene->ocean;

			auto sun = scene->sky.sun.get();
			if (!sun) {
				auto lights = scene->GetLights();
				for (auto& light : lights) {
					if (light->type == AE_DIRECTIONAL_LIGHT) {
						sun = static_cast<Lighting::DirectionalLight*>(light);
					}
				}

				if (!sun) return;
			}

			vec3 direction = normalize(sun->direction);

            /*
			{
                Graphics::Profiler::BeginQuery("Caustics");

				const int32_t groupSize = 8;
				auto res = ivec2(target->GetWidth(), target->GetHeight());

				ivec2 groupCount = res / groupSize;
				groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
				groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

				causticsShader.Bind();

				target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_READ_WRITE, 1);
				target->lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(0);

				causticsShader.GetUniform("waterHeight")->SetValue(ocean->translation.y);

				causticsShader.GetUniform("light.intensity")->SetValue(sun->intensity);
				causticsShader.GetUniform("light.direction")->SetValue(direction);
				causticsShader.GetUniform("light.color")->SetValue(sun->color);

				if (sun->GetShadow()) {
					auto distance = !sun->GetShadow()->longRange ? sun->GetShadow()->distance :
						sun->GetShadow()->longRangeDistance;

					causticsShader.GetUniform("light.shadow.distance")->SetValue(distance);
					causticsShader.GetUniform("light.shadow.bias")->SetValue(sun->GetShadow()->bias);
					causticsShader.GetUniform("light.shadow.cascadeCount")->SetValue(sun->GetShadow()->componentCount);
					causticsShader.GetUniform("light.shadow.resolution")->SetValue(vec2((float)sun->GetShadow()->resolution));

					sun->GetShadow()->maps.Bind(8);

					for (int32_t i = 0; i < sun->GetShadow()->componentCount; i++) {
						auto cascade = &sun->GetShadow()->components[i];
						auto frustum = Volume::Frustum(cascade->frustumMatrix);
						auto corners = frustum.GetCorners();
						auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
							abs(corners[1].y - corners[3].y)) / (float)sun->GetShadow()->resolution;
						auto lightSpace = cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix;
						causticsShader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].distance")->SetValue(cascade->farDistance);
						causticsShader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].cascadeSpace")->SetValue(lightSpace);
						causticsShader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].texelSize")->SetValue(texelSize);
					}
				}
				else {
					causticsShader.GetUniform("light.shadow.distance")->SetValue(0.0f);
				}

				glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

				glDispatchCompute(groupCount.x, groupCount.y, 1);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
            */

			// Update local texture copies
            {
                auto& colorImage = target->lightingFrameBuffer->GetColorImage(0);
                if (refractionTexture.width != colorImage->width ||
                    refractionTexture.height != colorImage->height ||
                    refractionTexture.format != colorImage->format) {
                    refractionTexture = Texture::Texture2D(colorImage->width, colorImage->height,
                        colorImage->format);
                }

                auto& depthImage = target->lightingFrameBuffer->GetDepthImage();
                if (depthTexture.width != depthImage->width ||
                    depthTexture.height != depthImage->height ||
                    depthTexture.format != depthImage->format) {
                    depthTexture = Texture::Texture2D(depthImage->width, depthImage->height,
                        depthImage->format);
                }

                std::vector<Graphics::ImageBarrier> imageBarriers;
                std::vector<Graphics::BufferBarrier> bufferBarriers;

                imageBarriers = {
                    {colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {depthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
                    {refractionTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                    {depthTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                commandList->CopyImage(colorImage, refractionTexture.image);
                commandList->CopyImage(depthImage, depthTexture.image);

                imageBarriers = {
                    {colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {depthImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {refractionTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                    {depthTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                };
                commandList->PipelineBarrier(imageBarriers, bufferBarriers,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }
			
			{
                Graphics::Profiler::EndAndBeginQuery("Surface");

                // TODO: Remove when simulation works
                commandList->ImageMemoryBarrier(ocean->simulation.displacementMap.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
                commandList->ImageMemoryBarrier(ocean->simulation.normalMap.image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

                commandList->BeginRenderPass(target->lightingFrameBuffer->renderPass, target->lightingFrameBuffer);

                auto config = GeneratePipelineConfig(target, ocean->wireframe);
				auto pipeline = PipelineManager::GetPipeline(config);

                commandList->BindPipeline(pipeline);

				vertexArray.Bind(commandList);

                Light lightUniform;
                lightUniform.direction = vec4(sun->direction, 0.0);
                lightUniform.color = vec4(sun->color, 0.0);
                lightUniform.intensity = sun->intensity;

				if (sun->GetVolumetric()) {
					target->volumetricTexture.Bind(commandList, 3, 7);
				}

                auto shadow = sun->GetShadow();
				if (shadow) {
					auto distance = !shadow->longRange ? shadow->distance :
						shadow->longRangeDistance;
                    auto& shadowUniform = lightUniform.shadow;
                    shadowUniform.distance = distance;
                    shadowUniform.bias = shadow->bias;
                    shadowUniform.cascadeCount = shadow->componentCount;
                    shadowUniform.resolution = vec2(shadow->resolution);

					shadow->maps.Bind(commandList, 3, 8);

					for (int32_t i = 0; i < sun->GetShadow()->componentCount; i++) {
						auto& cascade = shadow->components[i];
                        auto& cascadeUniform = shadowUniform.cascades[i];
						auto frustum = Volume::Frustum(cascade.frustumMatrix);
						auto corners = frustum.GetCorners();
						auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
							abs(corners[1].y - corners[3].y)) / (float)sun->GetShadow()->resolution;
                        cascadeUniform.distance = cascade.farDistance;
                        cascadeUniform.cascadeSpace = cascade.projectionMatrix * cascade.viewMatrix * camera->invViewMatrix;
                        cascadeUniform.texelSize = texelSize;
					}
				}
				else {
					shadow->distance = 0.0f;
				}

                lightUniformBuffer.SetData(&lightUniform, 0, 1);

                Uniforms uniforms = {
                    .translation = vec4(ocean->translation, 1.0f),

                    .displacementScale = ocean->displacementScale,
                    .choppyScale = ocean->choppynessScale,
                    .tiling = ocean->tiling,
                    .hasRippleTexture = ocean->rippleTexture.IsValid() ? 1 : 0,

                    .shoreWaveDistanceOffset = ocean->shoreWaveDistanceOffset,
                    .shoreWaveDistanceScale = ocean->shoreWaveDistanceScale,
                    .shoreWaveAmplitude = ocean->shoreWaveAmplitude,
                    .shoreWaveSteepness = ocean->shoreWaveSteepness,

                    .shoreWavePower = ocean->shoreWavePower,
                    .shoreWaveSpeed = ocean->shoreWaveSpeed,
                    .shoreWaveLength = ocean->shoreWaveLength,
                    .terrainSideLength = -1.0f,
                };

				ocean->simulation.displacementMap.Bind(commandList, 3, 0);
				ocean->simulation.normalMap.Bind(commandList, 3, 1);

				ocean->foamTexture.Bind(commandList, 3, 2);

				if (scene->sky.GetProbe()) {
                    scene->sky.GetProbe()->cubemap.Bind(commandList, 3, 3);
                }

				refractionTexture.Bind(commandList, 3, 4);
				depthTexture.Bind(commandList, 3, 5);

				if (scene->terrain) {
					if (scene->terrain->shoreLine.IsValid()) {
                        auto terrain = scene->terrain;

                        uniforms.terrainTranslation = vec4(terrain->translation, 1.0f);
                        uniforms.terrainSideLength = scene->terrain->sideLength;
                        uniforms.terrainHeightScale = scene->terrain->heightScale;

						scene->terrain->shoreLine.Bind(commandList, 3, 9);

					}
				}

				if (ocean->rippleTexture.IsValid()) {
					ocean->rippleTexture.Bind(commandList, 3, 10);
				}

                uniformBuffer.SetData(&uniforms, 0, 1);

                uniformBuffer.Bind(commandList, 3, 11);
                lightUniformBuffer.Bind(commandList, 3, 12);

				auto renderList = ocean->GetRenderList();

				for (auto node : renderList) {

                    PushConstants constants = {
                        .nodeSideLength = node->sideLength,
                        .nodeLocation = node->location,

                        .leftLoD = node->leftLoDStitch,
                        .topLoD = node->topLoDStitch,
                        .rightLoD = node->rightLoDStitch,
                        .bottomLoD = node->bottomLoDStitch
                    };

                    commandList->PushConstants("constants", &constants);

                    commandList->DrawIndexed(vertexArray.GetIndexComponent().elementCount);

				}

                commandList->EndRenderPass();

				Graphics::Profiler::EndQuery();

			}

			Graphics::Profiler::EndQuery();

		}

        PipelineConfig OceanRenderer::GeneratePipelineConfig(RenderTarget* target, bool wireframe) {

            const auto shaderConfig = ShaderConfig {
                {"ocean/ocean.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"ocean/ocean.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = target->lightingFrameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            pipelineDesc.rasterizer.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }


	}

}