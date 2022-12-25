#include "DirectionalLightRenderer.h"

#include "../lighting/DirectionalLight.h"

namespace Atlas {

	namespace Renderer {

		DirectionalLightRenderer::DirectionalLightRenderer() {

            /*
			shader.AddStage(AE_VERTEX_STAGE, "deferred/directional.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "deferred/directional.fsh");

			shader.AddMacro("SHADOWS");
			shader.AddMacro("SSAO");

			shader.Compile();

			GetUniforms();
             */

		}

		void DirectionalLightRenderer::Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) {

            /*
			Profiler::BeginQuery("Directional lights");

			shader.Bind();

			inverseViewMatrix->SetValue(camera->invViewMatrix);
			inverseProjectionMatrix->SetValue(camera->invProjectionMatrix);

			cameraLocation->SetValue(camera->GetLocation());

			glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);

			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(0);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT1)->Bind(1);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(2);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT3)->Bind(3);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT4)->Bind(4);
			target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(5);

			if (scene->sky.GetProbe()) {
				scene->sky.GetProbe()->cubemap.Bind(10);
				scene->sky.GetProbe()->filteredDiffuse.Bind(11);
			}

			auto lights = scene->GetLights();

			if (scene->sky.sun) {
				lights.push_back(scene->sky.sun);
			}

			// We will use two types of shaders: One with shadows and one without shadows (this is the only thing which might change per light)
			for (auto light : lights) {

				if (light->type != AE_DIRECTIONAL_LIGHT) {
					continue;
				}

				auto directionalLight = static_cast<Lighting::DirectionalLight*>(light);

				vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

				lightDirection->SetValue(direction);
				lightColor->SetValue(directionalLight->color);
				lightIntensity->SetValue(directionalLight->intensity);

				if (light->GetShadow()) {
					auto distance = !light->GetShadow()->longRange ? light->GetShadow()->distance :
						light->GetShadow()->longRangeDistance;
					shadowDistance->SetValue(distance);
					shadowBias->SetValue(directionalLight->GetShadow()->bias);
					shadowCascadeBlendDistance->SetValue(directionalLight->GetShadow()->cascadeBlendDistance);
					shadowCascadeCount->SetValue(directionalLight->GetShadow()->componentCount);
					shadowResolution->SetValue(vec2((float)directionalLight->GetShadow()->resolution));

					directionalLight->GetShadow()->maps.Bind(8);

					auto componentCount = directionalLight->GetShadow()->componentCount;

					for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
						if (i < componentCount) {
							auto cascade = &directionalLight->GetShadow()->components[i];
							auto frustum = Volume::Frustum(cascade->frustumMatrix);
							auto corners = frustum.GetCorners();
							auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
								abs(corners[1].y - corners[3].y)) / (float)light->GetShadow()->resolution;
							cascades[i].distance->SetValue(cascade->farDistance);
							cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix);
							cascades[i].texelSize->SetValue(texelSize);
						}
						else {
							auto cascade = &directionalLight->GetShadow()->components[componentCount - 1];
							cascades[i].distance->SetValue(cascade->farDistance);
						}
					}
				}
				else {
					shadowDistance->SetValue(0.0f);
				}

				glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			}

			Profiler::EndQuery();
             */

		}

		void DirectionalLightRenderer::GetUniforms() {

            /*
			inverseViewMatrix = shader.GetUniform("ivMatrix");
			inverseProjectionMatrix = shader.GetUniform("ipMatrix");

			cameraLocation = shader.GetUniform("cameraLocation");

			lightDirection = shader.GetUniform("light.direction");
			lightColor = shader.GetUniform("light.color");
			lightIntensity = shader.GetUniform("light.intensity");

			scatteringFactor = shader.GetUniform("light.scatteringFactor");

			shadowDistance = shader.GetUniform("light.shadow.distance");
			shadowBias = shader.GetUniform("light.shadow.bias");
			shadowCascadeBlendDistance = shader.GetUniform("light.shadow.cascadeBlendDistance");
			shadowCascadeCount = shader.GetUniform("light.shadow.cascadeCount");
			shadowResolution = shader.GetUniform("light.shadow.resolution");

			for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
				cascades[i].distance = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].distance");
				cascades[i].lightSpace = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].cascadeSpace");
				cascades[i].texelSize = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].texelSize");
			}

			fogScale = shader.GetUniform("fogScale");
			fogDistanceScale = shader.GetUniform("fogDistanceScale");
			fogHeight = shader.GetUniform("fogHeight");
			fogColor = shader.GetUniform("fogColor");
			fogScatteringPower = shader.GetUniform("fogScatteringPower");

			volumeMin = shader.GetUniform("volumeMin");
			volumeMax = shader.GetUniform("volumeMax");
			volumeProbeCount = shader.GetUniform("volumeProbeCount");
			volumeIrradianceRes = shader.GetUniform("volumeIrradianceRes");
			volumeMomentsRes = shader.GetUniform("volumeMomentsRes");
             */

		}

	}

}