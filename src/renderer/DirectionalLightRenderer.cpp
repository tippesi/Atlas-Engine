#include "DirectionalLightRenderer.h"
#include "MasterRenderer.h"

#include "../lighting/DirectionalLight.h"

namespace Atlas {

	namespace Renderer {

		std::string DirectionalLightRenderer::vertexPath = "deferred/directional.vsh";
		std::string DirectionalLightRenderer::fragmentPath = "deferred/directional.fsh";

		DirectionalLightRenderer::DirectionalLightRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.AddMacro("SHADOWS");

			shader.Compile();

			GetUniforms();

		}

		void DirectionalLightRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

			shader.Bind();

			diffuseTexture->SetValue(0);
			normalTexture->SetValue(1);
			materialTexture->SetValue(2);
			depthTexture->SetValue(3);
			aoTexture->SetValue(4);
			volumetricTexture->SetValue(5);
			shadowTexture->SetValue(6);

			inverseViewMatrix->SetValue(camera->inverseViewMatrix);
			inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

			target->geometryFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);
			target->geometryFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT1)->Bind(GL_TEXTURE1);
			target->geometryFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE2);
			target->geometryFramebuffer->GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE3);

			// We will use two types of shaders: One with shadows and one without shadows (this is the only thing which might change per light)
			for (auto& light : scene->lights) {

				if (light->type != AE_DIRECTIONAL_LIGHT || light->GetShadow() == nullptr) {
					continue;
				}

				auto directionalLight = (Lighting::DirectionalLight*)light;

				vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

				lightDirection->SetValue(direction);
				lightColor->SetValue(directionalLight->color);
				lightAmbient->SetValue(directionalLight->ambient);

				scatteringFactor->SetValue(directionalLight->GetVolumetric() != nullptr ? directionalLight->GetVolumetric()->scatteringFactor : 0.0f);

				shadowDistance->SetValue(directionalLight->GetShadow()->distance);
				shadowBias->SetValue(directionalLight->GetShadow()->bias);
				shadowSampleCount->SetValue(directionalLight->GetShadow()->sampleCount);
				shadowSampleRange->SetValue(directionalLight->GetShadow()->sampleRange);
				shadowCascadeCount->SetValue(directionalLight->GetShadow()->componentCount);
				shadowResolution->SetValue(vec2((float)directionalLight->GetShadow()->resolution));

				if (light->GetVolumetric() != nullptr) {
					glViewport(0, 0, directionalLight->GetVolumetric()->map->width, directionalLight->GetVolumetric()->map->height);
					directionalLight->GetVolumetric()->map->Bind(GL_TEXTURE5);
					glViewport(0, 0, target->lightingFramebuffer->width, target->lightingFramebuffer->height);
				}

				directionalLight->GetShadow()->maps->Bind(GL_TEXTURE6);

				for (int32_t i = 0; i < light->GetShadow()->componentCount; i++) {
					auto cascade = &directionalLight->GetShadow()->components[i];
					cascades[i].distance->SetValue(cascade->farDistance);
					cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
				}

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			}

		}

		void DirectionalLightRenderer::GetUniforms() {

			diffuseTexture = shader.GetUniform("diffuseTexture");
			normalTexture = shader.GetUniform("normalTexture");
			materialTexture = shader.GetUniform("materialTexture");
			depthTexture = shader.GetUniform("depthTexture");
			aoTexture = shader.GetUniform("aoTexture");
			volumetricTexture = shader.GetUniform("volumetricTexture");
			shadowTexture = shader.GetUniform("cascadeMaps");

			inverseViewMatrix = shader.GetUniform("ivMatrix");
			inverseProjectionMatrix = shader.GetUniform("ipMatrix");

			lightDirection = shader.GetUniform("light.direction");
			lightColor = shader.GetUniform("light.color");
			lightAmbient = shader.GetUniform("light.ambient");

			scatteringFactor = shader.GetUniform("light.scatteringFactor");

			shadowDistance = shader.GetUniform("light.shadow.distance");
			shadowBias = shader.GetUniform("light.shadow.bias");
			shadowSampleCount = shader.GetUniform("light.shadow.sampleCount");
			shadowSampleRange = shader.GetUniform("light.shadow.sampleRange");
			shadowSampleRandomness = shader.GetUniform("light.shadow.sampleRandomness");
			shadowCascadeCount = shader.GetUniform("light.shadow.cascadeCount");
			shadowResolution = shader.GetUniform("light.shadow.resolution");

			for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
				cascades[i].distance = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].distance");
				cascades[i].lightSpace = shader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].cascadeSpace");
			}

		}

	}

}