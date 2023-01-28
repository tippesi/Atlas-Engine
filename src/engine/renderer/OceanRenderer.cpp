#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		OceanRenderer::OceanRenderer() {

            /*
			Helper::GeometryHelper::GenerateGridVertexArray(vertexArray, 129, 1.0f / 128.0f);

			causticsShader.AddStage(AE_COMPUTE_STAGE, "ocean/caustics.csh");
			causticsShader.Compile();

			shader.AddStage(AE_VERTEX_STAGE, "ocean/ocean.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "ocean/ocean.fsh");
			shader.Compile();

			GetUniforms();
             */

		}

		void OceanRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            /*
			if (!scene->ocean || !scene->ocean->enable)
				return;

			Profiler::BeginQuery("Ocean");

			auto ocean = scene->ocean;

			auto sun = scene->sky.sun;
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

			{
				Profiler::BeginQuery("Caustics");

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

				causticsShader.GetUniform("time")->SetValue(Clock::Get());
				causticsShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
				causticsShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);

				glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

				glDispatchCompute(groupCount.x, groupCount.y, 1);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// Update local texture copies
			auto texture = target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0);

			if (refractionTexture.width != texture->width ||
				refractionTexture.height != texture->height ||
				refractionTexture.GetSizedFormat() != texture->GetSizedFormat()) {
				refractionTexture = Texture::Texture2D(*texture);
			}
			else {
				refractionTexture.Copy(*texture);
			}

			texture = target->lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT);

			if (depthTexture.width != texture->width ||
				depthTexture.height != texture->height ||
				depthTexture.GetSizedFormat() != texture->GetSizedFormat()) {
				depthTexture = Texture::Texture2D(*texture);
			}
			else {
				depthTexture.Copy(*texture);
			}
			
			{
				Profiler::EndAndBeginQuery("Surface");

				shader.Bind();

				vertexArray.Bind();

				shader.GetUniform("light.intensity")->SetValue(sun->intensity);
				lightDirection->SetValue(direction);
				lightColor->SetValue(sun->color);
				lightAmbient->SetValue(0.0f);

				if (sun->GetVolumetric()) {
					ivec2 res = ivec2(target->volumetricTexture.width, target->volumetricTexture.height);
					glViewport(0, 0, res.x, res.x);
					target->volumetricTexture.Bind(7);
					glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);
				}

				if (sun->GetShadow()) {
					auto distance = !sun->GetShadow()->longRange ? sun->GetShadow()->distance :
						sun->GetShadow()->longRangeDistance;
					shadowDistance->SetValue(distance);
					shadowBias->SetValue(sun->GetShadow()->bias);
					shadowCascadeCount->SetValue(sun->GetShadow()->componentCount);
					shadowResolution->SetValue(vec2((float)sun->GetShadow()->resolution));

					sun->GetShadow()->maps.Bind(8);

					for (int32_t i = 0; i < sun->GetShadow()->componentCount; i++) {
						auto cascade = &sun->GetShadow()->components[i];
						auto frustum = Volume::Frustum(cascade->frustumMatrix);
						auto corners = frustum.GetCorners();
						auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
							abs(corners[1].y - corners[3].y)) / (float)sun->GetShadow()->resolution;
						cascades[i].distance->SetValue(cascade->farDistance);
						cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix);
						shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].texelSize")->SetValue(texelSize);
					}
				}
				else {
					shadowDistance->SetValue(0.0f);
				}

				time->SetValue(Clock::Get());

				translation->SetValue(ocean->translation);

				displacementScale->SetValue(ocean->displacementScale);
				choppyScale->SetValue(ocean->choppynessScale);
				tiling->SetValue(ocean->tiling);

				shoreWaveDistanceOffset->SetValue(ocean->shoreWaveDistanceOffset);
				shoreWaveDistanceScale->SetValue(ocean->shoreWaveDistanceScale);
				shoreWaveAmplitude->SetValue(ocean->shoreWaveAmplitude);
				shoreWaveSteepness->SetValue(ocean->shoreWaveSteepness);
				shoreWavePower->SetValue(ocean->shoreWavePower);
				shoreWaveSpeed->SetValue(ocean->shoreWaveSpeed);
				shoreWaveLength->SetValue(ocean->shoreWaveLength);

				ocean->simulation.displacementMap.Bind(0);
				ocean->simulation.normalMap.Bind(1);

				ocean->foamTexture.Bind(2);

				if (scene->sky.GetProbe())
					scene->sky.GetProbe()->cubemap.Bind(3);

				refractionTexture.Bind(4);
				depthTexture.Bind(5);

				// In case a terrain isn't available
				terrainSideLength->SetValue(-1.0f);

				if (scene->terrain) {
					if (scene->terrain->shoreLine.width > 0 &&
						scene->terrain->shoreLine.height > 0) {

						terrainTranslation->SetValue(scene->terrain->translation);
						terrainHeightScale->SetValue(scene->terrain->heightScale);
						terrainSideLength->SetValue(scene->terrain->sideLength);

						scene->terrain->shoreLine.Bind(9);

					}
				}

				if (ocean->rippleTexture.width > 0 &&
					ocean->rippleTexture.height > 0) {
					ocean->rippleTexture.Bind(10);
					hasRippleTexture->SetValue(true);
				}
				else {
					hasRippleTexture->SetValue(false);
				}

				viewMatrix->SetValue(camera->viewMatrix);
				projectionMatrix->SetValue(camera->projectionMatrix);

				inverseViewMatrix->SetValue(camera->invViewMatrix);
				inverseProjectionMatrix->SetValue(camera->invProjectionMatrix);

				jitterLast->SetValue(camera->GetLastJitter());
				jitterCurrent->SetValue(camera->GetJitter());
				pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());

				cameraLocation->SetValue(camera->GetLocation());

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				auto renderList = ocean->GetRenderList();

				glDisable(GL_CULL_FACE);

#ifdef AE_API_GL
				if (ocean->wireframe)
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

				for (auto node : renderList) {

					nodeLocation->SetValue(node->location);
					nodeSideLength->SetValue(node->sideLength);

					leftLoD->SetValue(node->leftLoDStitch);
					topLoD->SetValue(node->topLoDStitch);
					rightLoD->SetValue(node->rightLoDStitch);
					bottomLoD->SetValue(node->bottomLoDStitch);

					glDrawElements(GL_TRIANGLE_STRIP, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
						vertexArray.GetIndexComponent()->GetDataType(), nullptr);

				}

#ifdef AE_API_GL
				if (ocean->wireframe)
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

				glEnable(GL_CULL_FACE);

				Profiler::EndQuery();
			}

			Profiler::EndQuery();
             */

		}

		void OceanRenderer::GetUniforms() {

            /*
			nodeLocation = shader.GetUniform("nodeLocation");
			nodeSideLength = shader.GetUniform("nodeSideLength");

			viewMatrix = shader.GetUniform("vMatrix");
			inverseViewMatrix = shader.GetUniform("ivMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			inverseProjectionMatrix = shader.GetUniform("ipMatrix");

			cameraLocation = shader.GetUniform("cameraLocation");

			time = shader.GetUniform("time");

			translation = shader.GetUniform("translation");

			displacementScale = shader.GetUniform("displacementScale");
			choppyScale = shader.GetUniform("choppyScale");
			tiling = shader.GetUniform("tiling");

			shoreWaveDistanceOffset = shader.GetUniform("shoreWaveDistanceOffset");
			shoreWaveDistanceScale = shader.GetUniform("shoreWaveDistanceScale");
			shoreWaveAmplitude = shader.GetUniform("shoreWaveAmplitude");
			shoreWaveSteepness = shader.GetUniform("shoreWaveSteepness");
			shoreWavePower = shader.GetUniform("shoreWavePower");
			shoreWaveSpeed = shader.GetUniform("shoreWaveSpeed");
			shoreWaveLength = shader.GetUniform("shoreWaveLength");

			leftLoD = shader.GetUniform("leftLoD");
			topLoD = shader.GetUniform("topLoD");
			rightLoD = shader.GetUniform("rightLoD");
			bottomLoD = shader.GetUniform("bottomLoD");

			lightDirection = shader.GetUniform("light.direction");
			lightColor = shader.GetUniform("light.color");
			lightAmbient = shader.GetUniform("light.ambient");

			shadowDistance = shader.GetUniform("light.shadow.distance");
			shadowBias = shader.GetUniform("light.shadow.bias");
			shadowCascadeCount = shader.GetUniform("light.shadow.cascadeCount");
			shadowResolution = shader.GetUniform("light.shadow.resolution");

			terrainTranslation = shader.GetUniform("terrainTranslation");
			terrainSideLength = shader.GetUniform("terrainSideLength");
			terrainHeightScale = shader.GetUniform("terrainHeightScale");

			hasRippleTexture = shader.GetUniform("hasRippleTexture");

			fogScale = shader.GetUniform("fogScale");
			fogDistanceScale = shader.GetUniform("fogDistanceScale");
			fogHeight = shader.GetUniform("fogHeight");
			fogColor = shader.GetUniform("fogColor");
			fogScatteringPower = shader.GetUniform("fogScatteringPower");

			for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
				cascades[i].distance = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].distance");
				cascades[i].lightSpace = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].cascadeSpace");
			}

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");
             */

		}

	}

}