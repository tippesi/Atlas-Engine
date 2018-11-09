#include "DirectionalLightRenderer.h"
#include "MasterRenderer.h"

DirectionalLightRenderer::DirectionalLightRenderer(const char* vertexSource, const char* fragmentSource) {

	rectangleVAO = MasterRenderer::GenerateRectangleVAO();

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	//shader->AddMacro("SHADOWS");
	//shader->AddMacro("SHADOW_FILTERING");

	shader->Compile();

	GetUniforms(false);

}

void DirectionalLightRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	shader->Bind();

	inverseViewMatrix->SetValue(camera->inverseViewMatrix);
	inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

	// We will use two types of shaders: One with shadows and one without shadows (this is the only thing which might change per light)
	for (Light* light : scene->lights) {

		/*
		if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr) {
			continue;
		}
		*/

		vec3 direction = normalize(vec3(camera->viewMatrix * vec4(light->direction, 0.0f)));

		lightDirection->SetValue(direction);
		lightColor->SetValue(light->diffuseColor);
		lightAmbient->SetValue(light->ambient);

		mat4 lightSpace = light->shadow->cascades[0].projectionMatrix * light->shadow->cascades[0].viewMatrix * camera->inverseViewMatrix;

		lightSpaceMatrix->SetValue(lightSpace);
		shadowDistance->SetValue(light->shadow->distance);
		shadowBias->SetValue(light->shadow->bias);
		shadowSampleCount->SetValue(light->shadow->sampleCount);
		shadowSampleRange->SetValue(light->shadow->sampleRange);

		target->geometryFramebuffer->components[0]->Bind(GL_TEXTURE0);
		target->geometryFramebuffer->components[1]->Bind(GL_TEXTURE1);
		target->geometryFramebuffer->components[2]->Bind(GL_TEXTURE2);
		//light->shadow->cascades[0].map->components[0]->Bind(GL_TEXTURE5);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	}

}

void DirectionalLightRenderer::GetUniforms(bool deleteUniforms) {

	if (deleteUniforms) {
		delete diffuseTexture;
		delete normalTexture;
		delete materialTexture;
		delete depthTexture;
		delete aoTexture;
		delete firstCascadeTexture;
		delete lightDirection;
		delete lightColor;
		delete lightAmbient;
		delete lightSpaceMatrix;
		delete inverseViewMatrix;
		delete inverseProjectionMatrix;
		delete shadowDistance;
		delete shadowBias;
		delete shadowSampleCount;
		delete shadowSampleRange;
		delete shadowSampleRandomness;
	}

	diffuseTexture = shader->GetUniform("diffuseTexture");
	normalTexture = shader->GetUniform("normalTexture");
	materialTexture = shader->GetUniform("materialTexture");
	depthTexture = shader->GetUniform("depthTexture");
	aoTexture = shader->GetUniform("aoTexture");
	firstCascadeTexture = shader->GetUniform("firstCascadeTexture");
	lightDirection = shader->GetUniform("light.direction");
	lightColor = shader->GetUniform("light.color");
	lightAmbient = shader->GetUniform("light.ambient");
	lightSpaceMatrix = shader->GetUniform("lightSpaceMatrix");
	inverseViewMatrix = shader->GetUniform("ivMatrix");
	inverseProjectionMatrix = shader->GetUniform("ipMatrix");
	shadowDistance = shader->GetUniform("light.shadow.distance");
	shadowBias = shader->GetUniform("light.shadow.bias");
	shadowSampleCount = shader->GetUniform("light.shadow.sampleCount");
	shadowSampleRange = shader->GetUniform("light.shadow.sampleRange");
	shadowSampleRandomness = shader->GetUniform("light.shadow.sampleRandomness");

	diffuseTexture->SetValue(0);
	normalTexture->SetValue(1);
	materialTexture->SetValue(2);
	depthTexture->SetValue(3);
	aoTexture->SetValue(4);
	firstCascadeTexture->SetValue(5);

}