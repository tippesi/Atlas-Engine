#include "PathTracingRenderer.h"
#include "../Log.h"
#include "../Clock.h"
#include "../common/Packing.h"
#include "../common/RandomHelper.h"
#include "../shader/PipelineManager.h"

#include "../volume/BVH.h"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		void PathTracingRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            rayGenPipelineConfig = PipelineConfig("pathtracer/rayGen.csh");
            rayHitPipelineConfig = PipelineConfig("pathtracer/rayHit.csh");

            Graphics::BufferDesc bufferDesc {
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = sizeof(RayGenUniforms)
            };
            rayGenUniformBuffer = device->CreateMultiBuffer(bufferDesc);

            bufferDesc.size = sizeof(RayHitUniforms);
            rayHitUniformBuffers.resize(bounces + 1);
            for (uint32_t i = 0; i < uint32_t(rayHitUniformBuffers.size()); i++) {
                rayHitUniformBuffers[i] = device->CreateMultiBuffer(bufferDesc);
            }

		}

		void PathTracingRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {



		}

		void PathTracingRenderer::Render(Viewport* viewport, PathTracerRenderTarget* renderTarget,
			ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene, Graphics::CommandList* commandList) {

			Graphics::Profiler::BeginQuery("Path tracing");

			auto width = renderTarget->GetWidth();
			auto height = renderTarget->GetHeight();

			if (glm::distance(camera->GetLocation(), cameraLocation) > 1e-3f ||
				glm::distance(camera->rotation, cameraRotation) > 1e-3f ||
				helper.GetRayBuffer()->GetElementCount() != 2 * width * height) {
				cameraLocation = camera->GetLocation();
				cameraRotation = camera->rotation;

				sampleCount = 0;
				imageOffset = ivec2(0);
				helper.SetRayBufferSize(width * height);
			}

			// Check if the scene has changed. A change might happen when an actor has been updated,
			// new actors have been added or old actors have been removed. If this happens we update
			// the data structures.
			helper.SetScene(scene, 1, true);
			helper.UpdateLights();

			ivec2 resolution = ivec2(width, height);
			ivec2 tileSize = resolution / imageSubdivisions;

			for (int32_t i = 0; i <= bounces; i++) {
				RayHitUniforms uniforms;
				uniforms.maxBounces = bounces;

				uniforms.sampleCount = sampleCount;
				uniforms.bounceCount = i;

				uniforms.resolution = resolution;
				uniforms.seed = Common::Random::SampleFastUniformFloat();

				uniforms.exposure = camera->exposure;

				rayHitUniformBuffers[i]->SetData(&uniforms, 0, sizeof(RayHitUniforms));
			}

			// Bind texture only for writing
            commandList->ImageMemoryBarrier(renderTarget->texture.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_WRITE_BIT);
            commandList->BindImage(renderTarget->texture.image, 3, 1);
			if (sampleCount % 2 == 0) {
                commandList->ImageMemoryBarrier(renderTarget->accumTexture0.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT);
                commandList->ImageMemoryBarrier(renderTarget->accumTexture1.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT);
                commandList->BindImage(renderTarget->accumTexture0.image, 3, 2);
                commandList->BindImage(renderTarget->accumTexture1.image, 3, 3);
			}
			else {
                commandList->ImageMemoryBarrier(renderTarget->accumTexture0.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT);
                commandList->ImageMemoryBarrier(renderTarget->accumTexture1.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT);
                commandList->BindImage(renderTarget->accumTexture0.image, 3, 3);
                commandList->BindImage(renderTarget->accumTexture1.image, 3, 2);
			}

			auto tileResolution = resolution / imageSubdivisions;
			auto groupCount = tileResolution / 8;

			groupCount.x += ((groupCount.x * 8 == tileResolution.x) ? 0 : 1);
			groupCount.y += ((groupCount.y * 8 == tileResolution.y) ? 0 : 1);

            Graphics::Profiler::BeginQuery("Ray generation");

			helper.DispatchRayGen(commandList, PipelineManager::GetPipeline(rayGenPipelineConfig),
                ivec3(groupCount.x, groupCount.y, 1), false,
				[=]() {
					auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

                    RayGenUniforms uniforms;
                    uniforms.origin = vec4(corners[4], 1.0f);
                    uniforms.right = vec4(corners[5] - corners[4], 1.0f);
                    uniforms.bottom = vec4(corners[6] - corners[4], 1.0f);

                    uniforms.sampleCount = sampleCount;
                    uniforms.pixelOffset = ivec2(renderTarget->GetWidth(),
                        renderTarget->GetHeight()) / imageSubdivisions * imageOffset;

                    uniforms.tileSize = tileSize;
                    uniforms.resolution = resolution;

                    rayGenUniformBuffer->SetData(&uniforms, 0, sizeof(RayGenUniforms));
                    commandList->BindBuffer(rayGenUniformBuffer, 3, 4);
				}
				);

			
			for (int32_t i = 0; i <= bounces; i++) {

                Graphics::Profiler::EndAndBeginQuery("Bounce " + std::to_string(i));

				helper.DispatchHitClosest(commandList, PipelineManager::GetPipeline(rayHitPipelineConfig), false,
					[=]() {
                        commandList->BindBuffer(rayHitUniformBuffers[i], 3, 4);
					}
					);
			}

            Graphics::Profiler::EndQuery();

			imageOffset.x++;

			if (imageOffset.x == imageSubdivisions.x) {
				imageOffset.x = 0;
				imageOffset.y++;
			}

			if (imageOffset.y == imageSubdivisions.y) {
				imageOffset.y = 0;
				sampleCount++;
			}

            commandList->ImageTransition(renderTarget->texture.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

			helper.InvalidateRayBuffer(commandList);

            Graphics::Profiler::EndQuery();

		}

		void PathTracingRenderer::ResetSampleCount() {

			sampleCount = 0;

		}

		int32_t PathTracingRenderer::GetSampleCount() const {

			return sampleCount;

		}

	}

}