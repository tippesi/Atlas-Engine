#include "OceanRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

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

			vec3 direction = normalize(sun->direction);

			lightDirection->SetValue(direction);
			lightColor->SetValue(sun->color);
			lightAmbient->SetValue(sun->ambient);
			
			lightScatteringFactor->SetValue(sun->GetVolumetric() ? sun->GetVolumetric()->scatteringFactor : 0.0f);

			if (sun->GetVolumetric()) {
				glViewport(0, 0, sun->GetVolumetric()->map.width, sun->GetVolumetric()->map.height);
				sun->GetVolumetric()->map.Bind(GL_TEXTURE7);
				glViewport(0, 0, target->lightingFramebuffer.width, target->lightingFramebuffer.height);
			}			

			if (sun->GetShadow()) {
				auto distance = !sun->GetShadow()->longRange ? sun->GetShadow()->distance :
					sun->GetShadow()->longRangeDistance;
				shadowDistance->SetValue(distance);
				shadowBias->SetValue(sun->GetShadow()->bias);
				shadowCascadeCount->SetValue(sun->GetShadow()->componentCount);
				shadowResolution->SetValue(vec2((float)sun->GetShadow()->resolution));

				sun->GetShadow()->maps.Bind(GL_TEXTURE8);

				for (int32_t i = 0; i < sun->GetShadow()->componentCount; i++) {
					auto cascade = &sun->GetShadow()->components[i];
					cascades[i].distance->SetValue(cascade->farDistance);
					cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix);
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

			ocean->simulation.displacementMapPrev.Bind(GL_TEXTURE10);

			// foam.Bind(GL_TEXTURE2);

			if (scene->sky.cubemap != nullptr)
				scene->sky.cubemap->Bind(GL_TEXTURE3);

			refractionTexture.Bind(GL_TEXTURE4);
			depthTexture.Bind(GL_TEXTURE5);

			// In case a terrain isn't available
			terrainSideLength->SetValue(-1.0f);

			if (scene->terrain) {
				if (scene->terrain->heightApproximation.width > 0 &&
					scene->terrain->heightApproximation.height > 0) {

					terrainTranslation->SetValue(scene->terrain->translation);
					terrainHeightScale->SetValue(scene->terrain->heightScale);
					terrainSideLength->SetValue(scene->terrain->sideLength);

					scene->terrain->heightApproximation.Bind(GL_TEXTURE8);

				}
			}

			if (ocean->rippleTexture.width > 0 &&
				ocean->rippleTexture.height > 0) {
				ocean->rippleTexture.Bind(GL_TEXTURE9);
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

			// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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

		}

	}

}