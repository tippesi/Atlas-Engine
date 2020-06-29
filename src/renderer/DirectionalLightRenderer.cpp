#include "DirectionalLightRenderer.h"
#include "MasterRenderer.h"

#include "../lighting/DirectionalLight.h"

namespace Atlas {

	namespace Renderer {

		DirectionalLightRenderer::DirectionalLightRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, "deferred/directional.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "deferred/directional.fsh");

			shader.AddMacro("SHADOWS");

			shader.Compile();

			GetUniforms();

		}

		void DirectionalLightRenderer::Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene, Texture::Texture2D* dfgTexture) {

			shader.Bind();

			inverseViewMatrix->SetValue(camera->invViewMatrix);
			inverseProjectionMatrix->SetValue(camera->invProjectionMatrix);

			cameraLocation->SetValue(camera->GetLocation());

			glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);

			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT1)->Bind(GL_TEXTURE1);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE2);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT3)->Bind(GL_TEXTURE3);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT4)->Bind(GL_TEXTURE4);
			target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE5);
			dfgTexture->Bind(GL_TEXTURE9);

			if (scene->sky.probe) {
				scene->sky.probe->cubemap.Bind(GL_TEXTURE10);
			}

			if (scene->sky.probe) {
				scene->sky.probe->filteredDiffuse.Bind(GL_TEXTURE11);
			}

			volumeMin->SetValue(vec3(0.0));
			volumeMax->SetValue(vec3(0.0));

			if (scene->irradianceVolume) {
				if (scene->irradianceVolume->bounceCount) {
					scene->irradianceVolume->irradianceArray.Bind(GL_TEXTURE12);
					scene->irradianceVolume->momentsArray.Bind(GL_TEXTURE13);
					volumeMin->SetValue(scene->irradianceVolume->aabb.min);
					volumeMax->SetValue(scene->irradianceVolume->aabb.max);
					volumeProbeCount->SetValue(scene->irradianceVolume->probeCount);
					volumeIrradianceRes->SetValue(scene->irradianceVolume->irrRes);
					volumeMomentsRes->SetValue(scene->irradianceVolume->momRes);
				}
			}

			auto lights = scene->GetLights();

			// We will use two types of shaders: One with shadows and one without shadows (this is the only thing which might change per light)
			for (auto& light : lights) {

				if (light->type != AE_DIRECTIONAL_LIGHT) {
					continue;
				}

				auto directionalLight = static_cast<Lighting::DirectionalLight*>(light);

				vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

				lightDirection->SetValue(direction);
				lightColor->SetValue(directionalLight->color);
				lightIntensity->SetValue(directionalLight->intensity);

				if (light->GetVolumetric()) {
					glViewport(0, 0, directionalLight->GetVolumetric()->map.width, directionalLight->GetVolumetric()->map.height);
					directionalLight->GetVolumetric()->map.Bind(GL_TEXTURE7);
				}

				if (light->GetShadow()) {
					auto distance = !light->GetShadow()->longRange ? light->GetShadow()->distance :
						light->GetShadow()->longRangeDistance;
					shadowDistance->SetValue(distance);
					shadowBias->SetValue(directionalLight->GetShadow()->bias);
					shadowCascadeBlendDistance->SetValue(directionalLight->GetShadow()->cascadeBlendDistance);
					shadowCascadeCount->SetValue(directionalLight->GetShadow()->componentCount);
					shadowResolution->SetValue(vec2((float)directionalLight->GetShadow()->resolution));

					directionalLight->GetShadow()->maps.Bind(GL_TEXTURE8);

					auto componentCount = directionalLight->GetShadow()->componentCount;

					for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
						if (i < componentCount) {
							auto cascade = &directionalLight->GetShadow()->components[i];
							auto frustum = Volume::Frustum(cascade->frustumMatrix);
							auto corners = frustum.GetCorners();
							auto texel = glm::max(abs(corners[0].x - corners[1].x),
								abs(corners[1].y - corners[3].y)) / (float)light->GetShadow()->resolution;
							cascades[i].distance->SetValue(cascade->farDistance);
							cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix);
							cascades[i].texelSize->SetValue(texel);
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

				if (scene->fog && scene->fog->enable) {

					auto& fog = scene->fog;

					fogScale->SetValue(fog->scale);
					fogDistanceScale->SetValue(fog->distanceScale);
					fogHeight->SetValue(fog->height);
					fogColor->SetValue(fog->color);
					fogScatteringPower->SetValue(fog->scatteringPower);

				}
				else {

					fogScale->SetValue(0.0f);
					fogDistanceScale->SetValue(1.0f);
					fogHeight->SetValue(1.0f);
					fogScatteringPower->SetValue(1.0f);

				}

				glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			}

		}

		void DirectionalLightRenderer::GetUniforms() {

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

		}

	}

}