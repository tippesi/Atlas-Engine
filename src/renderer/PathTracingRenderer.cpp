#include "PathTracingRenderer.h"
#include "../Log.h"
#include "../Clock.h"
#include "../common/Packing.h"

#include "../volume/BVH.h"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		PathTracingRenderer::PathTracingRenderer() {

			// Load shader stages from hard drive and compile the shader
			rayGenShader.AddStage(AE_COMPUTE_STAGE, "pathtracer/rayGen.csh");
			rayGenShader.Compile();

			// Retrieve uniforms
			GetRayGenUniforms();

			rayHitShader.AddStage(AE_COMPUTE_STAGE, "pathtracer/rayHit.csh");
			rayHitShader.Compile();

			GetRayHitUniforms();

		}

		void PathTracingRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {



		}

		void PathTracingRenderer::Render(Viewport* viewport, PathTracerRenderTarget* renderTarget,
			ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene) {

			if (glm::distance(camera->GetLocation(), cameraLocation) > 1e-3f ||
				glm::distance(camera->rotation, cameraRotation) > 1e-3f) {
				cameraLocation = camera->GetLocation();
				cameraRotation = camera->rotation;

				sampleCount = 0;
				imageOffset = ivec2(0);
				helper.SetRayBufferSize(renderTarget->GetWidth() * renderTarget->GetHeight());
			}

			// Check if the scene has changed. A change might happen when an actor has been updated,
			// new actors have been added or old actors have been removed. If this happens we update
			// the data structures.
			helper.SetScene(scene, 1, true);
			helper.UpdateLights();

			ivec2 resolution = ivec2(renderTarget->GetWidth(), renderTarget->GetHeight());
			ivec2 tileSize = resolution / imageSubdivisions;

			// Bind texture only for writing
			renderTarget->texture.Bind(GL_WRITE_ONLY, 1);
			if (sampleCount % 2 == 0) {
				renderTarget->accumTexture0.Bind(GL_READ_ONLY, 2);
				renderTarget->accumTexture1.Bind(GL_WRITE_ONLY, 3);
			}
			else {
				renderTarget->accumTexture1.Bind(GL_WRITE_ONLY, 2);
				renderTarget->accumTexture0.Bind(GL_READ_ONLY, 3);
			}

			auto tileResolution = resolution / imageSubdivisions;
			auto groupCount = tileResolution / 8;

			groupCount.x += ((groupCount.x * 8 == tileResolution.x) ? 0 : 1);
			groupCount.y += ((groupCount.y * 8 == tileResolution.y) ? 0 : 1);

			helper.DispatchRayGen(&rayGenShader, ivec3(groupCount.x, groupCount.y, 1),
				[=]() {
					auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

					cameraLocationRayGenUniform->SetValue(camera->GetLocation());

					originRayGenUniform->SetValue(corners[4]);
					rightRayGenUniform->SetValue(corners[5] - corners[4]);
					bottomRayGenUniform->SetValue(corners[6] - corners[4]);

					sampleCountRayGenUniform->SetValue(sampleCount);
					pixelOffsetRayGenUniform->SetValue(ivec2(renderTarget->GetWidth(),
						renderTarget->GetHeight()) / imageSubdivisions * imageOffset);

					tileSizeRayGenUniform->SetValue(tileSize);
					resolutionRayGenUniform->SetValue(resolution);
				}
				);

			
			for (int32_t i = 0; i <= bounces; i++) {
				helper.DispatchHitClosest(&rayHitShader,
					[=]() {
						maxBouncesRayHitUniform->SetValue(bounces);

						sampleCountRayHitUniform->SetValue(sampleCount);
						bounceCountRayHitUniform->SetValue(i);

						resolutionRayHitUniform->SetValue(resolution);
						seedRayHitUniform->SetValue(float(rand()) / float(RAND_MAX));

						rayHitShader.GetUniform("exposure")->SetValue(camera->exposure);
					}
					);
			}

			renderTarget->texture.Unbind();

			imageOffset.x++;

			if (imageOffset.x == imageSubdivisions.x) {
				imageOffset.x = 0;
				imageOffset.y++;
			}

			if (imageOffset.y == imageSubdivisions.y) {
				imageOffset.y = 0;
				sampleCount++;
			}

			helper.InvalidateRayBuffer();

		}

		void PathTracingRenderer::ResetSampleCount() {

			sampleCount = 0;

		}

		int32_t PathTracingRenderer::GetSampleCount() const {

			return sampleCount;

		}

		void PathTracingRenderer::GetRayGenUniforms() {

			cameraLocationRayGenUniform = rayGenShader.GetUniform("cameraLocation");

			originRayGenUniform = rayGenShader.GetUniform("origin");
			rightRayGenUniform = rayGenShader.GetUniform("right");
			bottomRayGenUniform = rayGenShader.GetUniform("bottom");

			sampleCountRayGenUniform = rayGenShader.GetUniform("sampleCount");
			pixelOffsetRayGenUniform = rayGenShader.GetUniform("pixelOffset");

			tileSizeRayGenUniform = rayGenShader.GetUniform("tileSize");
			resolutionRayGenUniform = rayGenShader.GetUniform("resolution");

		}

		void PathTracingRenderer::GetRayHitUniforms() {

			maxBouncesRayHitUniform = rayHitShader.GetUniform("maxBounces");

			sampleCountRayHitUniform = rayHitShader.GetUniform("sampleCount");
			bounceCountRayHitUniform = rayHitShader.GetUniform("bounceCount");

			resolutionRayHitUniform = rayHitShader.GetUniform("resolution");
			seedRayHitUniform = rayHitShader.GetUniform("seed");

		}

	}

}