#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string OceanRenderer::vertexPath = "ocean/ocean.vsh";
		std::string OceanRenderer::fragmentPath = "ocean/ocean.fsh";

		OceanRenderer::OceanRenderer() {

			Helper::GeometryHelper::GenerateGridVertexArray(vertexArray, 129, 1.0f / 128.0f);

			// foam = Texture::Texture2D("foam.jpg", false);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void OceanRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			if (!scene->ocean)
				return;

			auto ocean = scene->ocean;

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

			lightDirection->SetValue(normalize(sun->direction));
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

				sun->GetShadow()->maps.Bind(GL_TEXTURE6);

				for (int32_t i = 0; i < sun->GetShadow()->componentCount; i++) {
					auto cascade = &sun->GetShadow()->components[i];
					cascades[i].distance->SetValue(cascade->farDistance);
					cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
				}
			}
			else {
				shadowDistance->SetValue(0.0f);
			}

			displacementScale->SetValue(ocean->displacementScale);
			choppyScale->SetValue(ocean->choppynessScale);
			tiling->SetValue(ocean->tiling);

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

			ocean->simulation.displacementMap.Bind(GL_TEXTURE0);
			ocean->simulation.normalMap.Bind(GL_TEXTURE1);

			foam.Bind(GL_TEXTURE2);

			if (scene->sky.cubemap != nullptr)
				scene->sky.cubemap->Bind(GL_TEXTURE3);

			refractionTexture.Bind(GL_TEXTURE4);
			depthTexture.Bind(GL_TEXTURE5);

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);

			inverseViewMatrix->SetValue(camera->inverseViewMatrix);
			inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			auto renderList = ocean->GetRenderList();

			// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			oceanHeight->SetValue(ocean->height);

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

			// glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		}

		void OceanRenderer::GetUniforms() {

			nodeLocation = shader.GetUniform("nodeLocation");
			nodeSideLength = shader.GetUniform("nodeSideLength");
			oceanHeight = shader.GetUniform("oceanHeight");

			viewMatrix = shader.GetUniform("vMatrix");
			inverseViewMatrix = shader.GetUniform("ivMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			inverseProjectionMatrix = shader.GetUniform("ipMatrix");

			displacementScale = shader.GetUniform("displacementScale");
			choppyScale = shader.GetUniform("choppyScale");
			tiling = shader.GetUniform("tiling");

			leftLoD = shader.GetUniform("leftLoD");
			topLoD = shader.GetUniform("topLoD");
			rightLoD = shader.GetUniform("rightLoD");
			bottomLoD = shader.GetUniform("bottomLoD");

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