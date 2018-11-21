#include "DirectionalLightRenderer.h"
#include "MasterRenderer.h"

DirectionalLightRenderer::DirectionalLightRenderer(string vertexSource, string fragmentSource) {

	rectangleVAO = MasterRenderer::GenerateRectangleVAO();

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	shader->AddMacro("SHADOWS");
#ifdef ENGINE_OGL
	shader->AddMacro("SHADOW_FILTERING");
#endif

	shader->Compile();

	GetUniforms();

}

void DirectionalLightRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	shader->Bind();

	diffuseTexture->SetValue(0);
	normalTexture->SetValue(1);
	materialTexture->SetValue(2);
	depthTexture->SetValue(3);
	aoTexture->SetValue(4);
	volumetricTexture->SetValue(5);
	shadowTexture->SetValue(6);

	inverseViewMatrix->SetValue(camera->inverseViewMatrix);
	inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

	target->geometryFramebuffer->GetComponent(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);
	target->geometryFramebuffer->GetComponent(GL_COLOR_ATTACHMENT1)->Bind(GL_TEXTURE1);
	target->geometryFramebuffer->GetComponent(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE2);
	target->geometryFramebuffer->GetComponent(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE3);

	// We will use two types of shaders: One with shadows and one without shadows (this is the only thing which might change per light)
	for (Light* light : scene->lights) {

		if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr) {
			continue;
		}

		vec3 direction = normalize(vec3(camera->viewMatrix * vec4(light->direction, 0.0f)));

		lightDirection->SetValue(direction);
		lightColor->SetValue(light->diffuseColor);
		lightAmbient->SetValue(light->ambient);

		shadowDistance->SetValue(light->shadow->distance);
		shadowBias->SetValue(light->shadow->bias);
		shadowSampleCount->SetValue(light->shadow->sampleCount);
		shadowSampleRange->SetValue(light->shadow->sampleRange);
		shadowCascadeCount->SetValue(light->shadow->componentCount);
		shadowResolution->SetValue(vec2((float)light->shadow->resolution));

		if (light->volumetric != nullptr) {
			glViewport(0, 0, light->volumetric->map->width, light->volumetric->map->height);
			light->volumetric->map->Bind(GL_TEXTURE5);
			glViewport(0, 0, target->lightingFramebuffer->width, target->lightingFramebuffer->height);
		}

		light->shadow->maps->Bind(GL_TEXTURE6);
#ifdef ENGINE_OGL
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
#endif

		for (int32_t i = 0; i < light->shadow->componentCount; i++) {
			ShadowComponent* cascade = &light->shadow->components[i];
			cascades[i].distance->SetValue(cascade->farDistance);
			cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
		}

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	}

}

void DirectionalLightRenderer::GetUniforms() {

	diffuseTexture = shader->GetUniform("diffuseTexture");
	normalTexture = shader->GetUniform("normalTexture");
	materialTexture = shader->GetUniform("materialTexture");
	depthTexture = shader->GetUniform("depthTexture");
	aoTexture = shader->GetUniform("aoTexture");
	volumetricTexture = shader->GetUniform("volumetricTexture");
	shadowTexture = shader->GetUniform("cascadeMaps");
	lightDirection = shader->GetUniform("light.direction");
	lightColor = shader->GetUniform("light.color");
	lightAmbient = shader->GetUniform("light.ambient");
	inverseViewMatrix = shader->GetUniform("ivMatrix");
	inverseProjectionMatrix = shader->GetUniform("ipMatrix");
	shadowDistance = shader->GetUniform("light.shadow.distance");
	shadowBias = shader->GetUniform("light.shadow.bias");
	shadowSampleCount = shader->GetUniform("light.shadow.sampleCount");
	shadowSampleRange = shader->GetUniform("light.shadow.sampleRange");
	shadowSampleRandomness = shader->GetUniform("light.shadow.sampleRandomness");
	shadowCascadeCount = shader->GetUniform("light.shadow.cascadeCount");
	shadowResolution = shader->GetUniform("light.shadow.resolution");

	for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
		cascades[i].distance = shader->GetUniform("light.shadow.cascades[" + to_string(i) + "].distance");
		cascades[i].lightSpace = shader->GetUniform("light.shadow.cascades[" + to_string(i) + "].cascadeSpace");
	}

}