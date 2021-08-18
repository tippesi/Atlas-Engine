#include "IrradianceVolumeRenderer.h"

#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		DDGIRenderer::DDGIRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			rayHitBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(vec4),
				AE_BUFFER_DYNAMIC_STORAGE);

			rayGenShader.AddStage(AE_COMPUTE_STAGE, "ddgi/rayGen.csh");
			rayGenShader.Compile();

			rayHitShader.AddStage(AE_COMPUTE_STAGE, "ddgi/rayHit.csh");
			rayHitShader.Compile();

			probeStateShader.AddStage(AE_COMPUTE_STAGE, "ddgi/probeState.csh");
			probeStateShader.Compile();

			probeUpdateShader.AddStage(AE_COMPUTE_STAGE, "ddgi/probeUpdate.csh");
			probeUpdateShader.Compile();

			copyEdgeShader.AddStage(AE_VERTEX_STAGE, "ddgi/dummy.vsh");
			copyEdgeShader.AddStage(AE_FRAGMENT_STAGE, "ddgi/copyEdge.fsh");
			copyEdgeShader.Compile();

		}

		void DDGIRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {

		}

		void DDGIRenderer::TraceAndUpdateProbes(Scene::Scene* scene) {
			if (!scene->irradianceVolume)
				return;

			auto volume = scene->irradianceVolume;
			auto& internalVolume = volume->internal;
			internalVolume.SwapTextures();

			auto totalProbeCount = volume->probeCount.x *
				volume->probeCount.y *
				volume->probeCount.z;

			auto rayCount = volume->rayCount;
			auto rayCountInactive = volume->rayCountInactive;

			auto totalRayCount = rayCount * totalProbeCount;

			if (totalRayCount != rayHitBuffer.GetElementCount()) {
				helper.SetRayBufferSize(totalRayCount);
				rayHitBuffer.SetSize(totalRayCount);
				helper.SetScene(scene, 8);
			}

			// Currently the BVH structure doesn't support fast rebuilds
			//if (scene->HasChanged())
				//helper.SetScene(scene, 8);

			auto [irradianceArray, momentsArray] = internalVolume.GetCurrentProbes();
			auto [lastIrradianceArray, lastMomentsArray] = internalVolume.GetLastProbes();

			auto& rayDirBuffer = internalVolume.rayDirBuffer;
			auto& rayDirInactiveBuffer = internalVolume.rayDirInactiveBuffer;
			auto& probeStateBuffer = internalVolume.probeStateBuffer;
			auto& probeStateTemporalBuffer = internalVolume.probeStateTemporalBuffer;

			helper.UpdateLights();

			probeStateBuffer.BindBase(9);

			helper.DispatchRayGen(&rayGenShader, volume->probeCount,
				[&]() {
					auto theta = acosf(2.0f * float(rand()) / float(RAND_MAX) - 1.0f);
					auto phi = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);

					auto dir = glm::vec3(
						sinf(theta) * cosf(phi),
						sinf(theta) * sinf(phi),
						cosf(theta)
					);

					auto epsilon = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);
					auto rot = mat3(glm::rotate(epsilon, dir));

					rayGenShader.GetUniform("rayCount")->SetValue(rayCount);
					rayGenShader.GetUniform("rayCountInactive")->SetValue(rayCountInactive);
					rayGenShader.GetUniform("randomRotation")->SetValue(rot);

					rayGenShader.GetUniform("volumeMin")->SetValue(volume->aabb.min);
					rayGenShader.GetUniform("volumeMax")->SetValue(volume->aabb.max);
					rayGenShader.GetUniform("volumeProbeCount")->SetValue(volume->probeCount);
					rayGenShader.GetUniform("cellSize")->SetValue(volume->cellSize);

					rayDirBuffer.BindBase(4);
					rayDirInactiveBuffer.BindBase(5);
				}
			);

			lastIrradianceArray.Bind(GL_TEXTURE12);
			lastMomentsArray.Bind(GL_TEXTURE13);

			helper.DispatchHitClosest(&rayHitShader,
				[&]() {
					rayHitShader.GetUniform("seed")->SetValue(float(rand()) / float(RAND_MAX));

					rayHitShader.GetUniform("volumeMin")->SetValue(volume->aabb.min);
					rayHitShader.GetUniform("volumeMax")->SetValue(volume->aabb.max);
					rayHitShader.GetUniform("volumeProbeCount")->SetValue(volume->probeCount);

					rayHitShader.GetUniform("volumeBias")->SetValue(volume->bias);
					rayHitShader.GetUniform("volumeGamma")->SetValue(volume->gamma);
					rayHitShader.GetUniform("cellSize")->SetValue(volume->cellSize);

					rayHitShader.GetUniform("volumeIrradianceRes")->SetValue(volume->irrRes);
					rayHitShader.GetUniform("volumeMomentsRes")->SetValue(volume->momRes);

					// Use this buffer instead of the default writeRays buffer of the helper
					rayHitBuffer.BindBase(3);
				}
			);

			rayHitBuffer.BindBase(0);

			ivec3 probeCount = volume->probeCount;

			// Update the probes
			{
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

				irradianceArray.Bind(GL_WRITE_ONLY, 0);
				momentsArray.Bind(GL_WRITE_ONLY, 1);

				probeUpdateShader.Bind();
				probeUpdateShader.GetUniform("irrProbeRes")->SetValue(volume->irrRes);
				probeUpdateShader.GetUniform("momProbeRes")->SetValue(volume->momRes);
				probeUpdateShader.GetUniform("rayCount")->SetValue(rayCount);
				probeUpdateShader.GetUniform("rayCountInactive")->SetValue(rayCountInactive);
				probeUpdateShader.GetUniform("hysteresis")->SetValue(volume->hysteresis);
				probeUpdateShader.GetUniform("cellSize")->SetValue(volume->cellSize);
				probeUpdateShader.GetUniform("volumeGamma")->SetValue(volume->gamma);
				probeUpdateShader.GetUniform("depthSharpness")->SetValue(volume->sharpness);

				glDispatchCompute(probeCount.x, probeCount.y, probeCount.z);
			}

			// Update the states of the probes
			{
				probeStateShader.Bind();
				probeStateShader.GetUniform("rayCount")->SetValue(rayCount);
				probeStateShader.GetUniform("rayCountInactive")->SetValue(rayCountInactive);
				probeStateShader.GetUniform("cellLength")->SetValue(glm::length(volume->cellSize));
				probeStateShader.GetUniform("cellSize")->SetValue(volume->cellSize);

				probeStateTemporalBuffer.BindBase(1);

				glDispatchCompute(probeCount.x, probeCount.y, probeCount.z);
			}

			// Copy the probe edges
			{
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				for (int32_t y = 0; y < probeCount.y; y++) {

					irradianceFramebuffer.AddComponentTextureArray(GL_COLOR_ATTACHMENT0, 
						const_cast<Texture::Texture2DArray*>(&irradianceArray), y);
					momentsFramebuffer.AddComponentTextureArray(GL_COLOR_ATTACHMENT0, 
						const_cast<Texture::Texture2DArray*>(&momentsArray), y);

					vertexArray.Bind();

					glViewport(0, 0, irradianceArray.width, irradianceArray.height);

					irradianceFramebuffer.Bind();

					copyEdgeShader.Bind();
					copyEdgeShader.GetUniform("probeRes")->SetValue(volume->irrRes);
					copyEdgeShader.GetUniform("volumeLayer")->SetValue(y);

					irradianceArray.Bind(GL_TEXTURE0);

					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

					glViewport(0, 0, momentsArray.width, momentsArray.height);

					momentsFramebuffer.Bind();

					copyEdgeShader.Bind();
					copyEdgeShader.GetUniform("probeRes")->SetValue(volume->momRes);
					copyEdgeShader.GetUniform("volumeLayer")->SetValue(y);

					momentsArray.Bind(GL_TEXTURE0);

					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

				}
			}

			helper.InvalidateRayBuffer();

		}

	}

}