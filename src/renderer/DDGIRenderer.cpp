#include "DDGIRenderer.h"

#include "../common/RandomHelper.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		DDGIRenderer::DDGIRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);
			Helper::GeometryHelper::GenerateSphereVertexArray(sphereArray, 10, 10);

			rayHitBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(vec4),
				AE_BUFFER_DYNAMIC_STORAGE);

			rayGenShader.AddStage(AE_COMPUTE_STAGE, "ddgi/rayGen.csh");
			rayGenShader.Compile();

			rayHitShader.AddStage(AE_COMPUTE_STAGE, "ddgi/rayHit.csh");
			rayHitShader.Compile();

			probeStateShader.AddStage(AE_COMPUTE_STAGE, "ddgi/probeState.csh");
			probeStateShader.Compile();

			probeIrradianceUpdateShader.AddStage(AE_COMPUTE_STAGE, "ddgi/probeUpdate.csh");
			probeIrradianceUpdateShader.AddMacro("IRRADIANCE");
			probeIrradianceUpdateShader.Compile();

			probeMomentsUpdateShader.AddStage(AE_COMPUTE_STAGE, "ddgi/probeUpdate.csh");
			probeMomentsUpdateShader.Compile();

			copyEdgeShader.AddStage(AE_VERTEX_STAGE, "ddgi/dummy.vsh");
			copyEdgeShader.AddStage(AE_FRAGMENT_STAGE, "ddgi/copyEdge.fsh");
			copyEdgeShader.Compile();

			probeDebugShader.AddStage(AE_VERTEX_STAGE, "ddgi/probeDebug.vsh");
			probeDebugShader.AddStage(AE_FRAGMENT_STAGE, "ddgi/probeDebug.fsh");
			probeDebugShader.Compile();

		}

		void DDGIRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {

		}

		void DDGIRenderer::TraceAndUpdateProbes(Scene::Scene* scene) {
			auto volume = scene->irradianceVolume;
			if (!volume || !volume->enable || !volume->update)
				return;

			Profiler::BeginQuery("DDGI");
			
			auto& internalVolume = volume->internal;
			internalVolume.SwapTextures();

			auto totalProbeCount = volume->probeCount.x *
				volume->probeCount.y *
				volume->probeCount.z;

			auto rayCount = volume->rayCount;
			auto rayCountInactive = volume->rayCountInactive;

			auto totalRayCount = rayCount * totalProbeCount;

			Profiler::BeginQuery("Scene update");

			if (totalRayCount != rayHitBuffer.GetElementCount()) {
				helper.SetRayBufferSize(totalRayCount);
				rayHitBuffer.SetSize(totalRayCount);
				helper.SetScene(scene, 8);
			}

			auto [irradianceArray, momentsArray] = internalVolume.GetCurrentProbes();
			auto [lastIrradianceArray, lastMomentsArray] = internalVolume.GetLastProbes();

			auto& rayDirBuffer = internalVolume.rayDirBuffer;
			auto& rayDirInactiveBuffer = internalVolume.rayDirInactiveBuffer;
			auto& probeStateBuffer = internalVolume.probeStateBuffer;
			auto& probeOffsetBuffer = internalVolume.probeOffsetBuffer;

			helper.SetScene(scene, 8, volume->sampleEmissives);
			helper.UpdateLights();

			probeStateBuffer.BindBase(9);
			probeOffsetBuffer.BindBase(10);

			Profiler::EndAndBeginQuery("Ray generation");

			helper.DispatchRayGen(&rayGenShader, volume->probeCount,
				[&]() {
					using namespace Common;

					auto theta = acosf(2.0f * float(rand()) / float(RAND_MAX) - 1.0f);
					auto phi = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);

					auto dir = glm::vec3(
						2.0f * Random::CanonicalUniform() - 1.0f,
						2.0f * Random::CanonicalUniform() - 1.0f,
						2.0f * Random::CanonicalUniform() - 1.0f
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

			Profiler::EndAndBeginQuery("Ray evaluation");

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
			
			Profiler::EndAndBeginQuery("Update probes");

			ivec3 probeCount = volume->probeCount;

			// Update the probes
			{
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

				irradianceArray.Bind(GL_WRITE_ONLY, 0);
				momentsArray.Bind(GL_WRITE_ONLY, 1);

				probeIrradianceUpdateShader.Bind();
				probeIrradianceUpdateShader.GetUniform("irrProbeRes")->SetValue(volume->irrRes);
				probeIrradianceUpdateShader.GetUniform("momProbeRes")->SetValue(volume->momRes);
				probeIrradianceUpdateShader.GetUniform("rayCount")->SetValue(rayCount);
				probeIrradianceUpdateShader.GetUniform("rayCountInactive")->SetValue(rayCountInactive);
				probeIrradianceUpdateShader.GetUniform("hysteresis")->SetValue(volume->hysteresis);
				probeIrradianceUpdateShader.GetUniform("cellSize")->SetValue(volume->cellSize);
				probeIrradianceUpdateShader.GetUniform("volumeGamma")->SetValue(volume->gamma);
				probeIrradianceUpdateShader.GetUniform("depthSharpness")->SetValue(volume->sharpness);

				glDispatchCompute(probeCount.x, probeCount.y, probeCount.z);

				irradianceArray.Bind(GL_WRITE_ONLY, 0);
				momentsArray.Bind(GL_WRITE_ONLY, 1);

				probeMomentsUpdateShader.Bind();
				probeMomentsUpdateShader.GetUniform("irrProbeRes")->SetValue(volume->irrRes);
				probeMomentsUpdateShader.GetUniform("momProbeRes")->SetValue(volume->momRes);
				probeMomentsUpdateShader.GetUniform("rayCount")->SetValue(rayCount);
				probeMomentsUpdateShader.GetUniform("rayCountInactive")->SetValue(rayCountInactive);
				probeMomentsUpdateShader.GetUniform("hysteresis")->SetValue(volume->hysteresis);
				probeMomentsUpdateShader.GetUniform("cellSize")->SetValue(volume->cellSize);
				probeMomentsUpdateShader.GetUniform("volumeGamma")->SetValue(volume->gamma);
				probeMomentsUpdateShader.GetUniform("depthSharpness")->SetValue(volume->sharpness);
				probeMomentsUpdateShader.GetUniform("optimizeProbes")->SetValue(volume->optimizeProbes);

				glDispatchCompute(probeCount.x, probeCount.y, probeCount.z);
			}

			Profiler::EndAndBeginQuery("Update probe states");

			// Update the states of the probes
			{
				probeStateShader.Bind();
				probeStateShader.GetUniform("rayCount")->SetValue(rayCount);
				probeStateShader.GetUniform("rayCountInactive")->SetValue(rayCountInactive);
				probeStateShader.GetUniform("cellLength")->SetValue(glm::length(volume->cellSize));
				probeStateShader.GetUniform("cellSize")->SetValue(volume->cellSize);
				probeStateShader.GetUniform("hysteresis")->SetValue(volume->hysteresis);

				glDispatchCompute(probeCount.x, probeCount.y, probeCount.z);
			}

			Profiler::EndAndBeginQuery("Update probe edges");

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

			Profiler::EndQuery();
			Profiler::EndQuery();

		}

		void DDGIRenderer::DebugProbes(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene, std::unordered_map<void*, uint16_t>& materialMap) {

			auto volume = scene->irradianceVolume;
			if (!volume || !volume->enable || !volume->update || !volume->debug)
				return;

			glDisable(GL_CULL_FACE);
            
			probeDebugActiveMaterial.emissiveColor = vec3(0.0f, 1.0f, 0.0f);
			probeDebugInactiveMaterial.emissiveColor = vec3(1.0f, 0.0f, 0.0f);
			probeDebugOffsetMaterial.emissiveColor = vec3(0.0f, 0.0f, 1.0f);

			sphereArray.Bind();
			probeDebugShader.Bind();

			volume->internal.probeStateBuffer.BindBase(9);

			probeDebugShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			probeDebugShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
			probeDebugShader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());

			probeDebugShader.GetUniform("jitterLast")->SetValue(camera->GetLastJitter());
			probeDebugShader.GetUniform("jitterCurrent")->SetValue(camera->GetJitter());

			probeDebugShader.GetUniform("probeMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugMaterial]));
			probeDebugShader.GetUniform("probeActiveMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugActiveMaterial]));
			probeDebugShader.GetUniform("probeInactiveMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugInactiveMaterial]));
			probeDebugShader.GetUniform("probeOffsetMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugOffsetMaterial]));

			probeDebugShader.GetUniform("volumeMin")->SetValue(volume->aabb.min);
			probeDebugShader.GetUniform("volumeMax")->SetValue(volume->aabb.max);
			probeDebugShader.GetUniform("volumeProbeCount")->SetValue(volume->probeCount);

			probeDebugShader.GetUniform("cellSize")->SetValue(volume->cellSize);
			probeDebugShader.GetUniform("cellLength")->SetValue(glm::length(volume->cellSize));

			auto [irradianceArray, momentsArray] = volume->internal.GetCurrentProbes();
			irradianceArray.Bind(GL_TEXTURE12);
			momentsArray.Bind(GL_TEXTURE13);

			volume->internal.probeStateBuffer.BindBase(9);
			volume->internal.probeOffsetBuffer.BindBase(10);

			auto instanceCount = volume->probeCount.x * volume->probeCount.y * volume->probeCount.z;
			glDrawElementsInstanced(GL_TRIANGLES, (int32_t)sphereArray.GetIndexComponent()->GetElementCount(),
				sphereArray.GetIndexComponent()->GetDataType(), nullptr, instanceCount);

			glEnable(GL_CULL_FACE);

		}

	}

}