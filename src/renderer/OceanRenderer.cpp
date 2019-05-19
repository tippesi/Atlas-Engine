#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string OceanRenderer::vertexPath = "ocean/ocean.vsh";
		std::string OceanRenderer::fragmentPath = "ocean/ocean.fsh";

		OceanRenderer::OceanRenderer() {

			Helper::GeometryHelper::GenerateGridVertexArray(vertexArray, 512, 0.25f);

			simulation = new GPGPU::OceanSimulation(512, 2000);

			foam = Texture::Texture2D("foam.jpg", false);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void OceanRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			shader.Bind();

			vertexArray.Bind();

			auto lights = scene->GetLights();

			Lighting::DirectionalLight* sun = nullptr;

			for (auto& light : lights) {
				if (light->type == AE_DIRECTIONAL_LIGHT) {
					sun = static_cast<Lighting::DirectionalLight*>(light);
				}
			}

			if (!sun)
				return;

			vec3 direction = normalize(vec3(camera->viewMatrix * vec4(sun->direction, 0.0f)));

			lightDirection->SetValue(direction);
			lightColor->SetValue(sun->color);
			lightAmbient->SetValue(sun->ambient);

			
			lightScatteringFactor->SetValue(sun->GetVolumetric() ? sun->GetVolumetric()->scatteringFactor : 0.0f);

			if (sun->GetVolumetric()) {
				glViewport(0, 0, sun->GetVolumetric()->map->width, sun->GetVolumetric()->map->height);
				sun->GetVolumetric()->map->Bind(GL_TEXTURE7);
				glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);
			}
			

			if (sun->GetShadow()) {
				shadowDistance->SetValue(sun->GetShadow()->distance);
				shadowBias->SetValue(sun->GetShadow()->bias);
				shadowSampleCount->SetValue(sun->GetShadow()->sampleCount);
				shadowSampleRange->SetValue(sun->GetShadow()->sampleRange);
				shadowCascadeCount->SetValue(sun->GetShadow()->componentCount);
				shadowResolution->SetValue(vec2((float)sun->GetShadow()->resolution));

				sun->GetShadow()->maps->Bind(GL_TEXTURE6);

				for (int32_t i = 0; i < sun->GetShadow()->componentCount; i++) {
					auto cascade = &sun->GetShadow()->components[i];
					cascades[i].distance->SetValue(cascade->farDistance);
					cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
				}
			}
			else {
				shadowDistance->SetValue(0.0f);
			}

			displacementScale->SetValue(0.75f * 16.0f);
			choppyScale->SetValue(0.75f * 16.0f);
			cameraLocation->SetValue(vec3(camera->viewMatrix[3]));

			auto refractionTexture = Texture::Texture2D(
				*target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)
			);
			auto depthTexture = Texture::Texture2D(
				*target->lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)
			);

			simulation->displacementMap.Bind(GL_TEXTURE0);
			simulation->normalMap.Bind(GL_TEXTURE1);

			foam.Bind(GL_TEXTURE2);

			if (scene->sky.skybox != nullptr)
				scene->sky.skybox->cubemap->Bind(GL_TEXTURE3);

			refractionTexture.Bind(GL_TEXTURE4);
			depthTexture.Bind(GL_TEXTURE5);

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);

			inverseViewMatrix->SetValue(camera->inverseViewMatrix);
			inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
				GL_SHADER_STORAGE_BARRIER_BIT);

			glDrawElements(GL_TRIANGLE_STRIP, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
				vertexArray.GetIndexComponent()->GetDataType(), nullptr);

		}

		void OceanRenderer::Update() {

			simulation->Compute();

		}

		void OceanRenderer::GetUniforms() {

			viewMatrix = shader.GetUniform("vMatrix");
			inverseViewMatrix = shader.GetUniform("ivMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			inverseProjectionMatrix = shader.GetUniform("ipMatrix");

			cameraLocation = shader.GetUniform("cameraLocation");

			displacementScale = shader.GetUniform("displacementScale");
			choppyScale = shader.GetUniform("choppyScale");

			lightDirection = shader.GetUniform("light.direction");
			lightColor = shader.GetUniform("light.color");
			lightAmbient = shader.GetUniform("light.ambient");
			lightScatteringFactor = shader.GetUniform("light.scatteringFactor");

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