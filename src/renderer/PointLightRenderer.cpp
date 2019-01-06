#include "PointLightRenderer.h"
#include "helper/GeometryHelper.h"
#include "../lighting/PointLight.h"

string PointLightRenderer::vertexPath = "deferred/point.vsh";
string PointLightRenderer::fragmentPath = "deferred/point.fsh";

PointLightRenderer::PointLightRenderer() {

	vertexArray = GeometryHelper::GenerateSphereVertexArray(16, 16);

	shader.AddStage(VERTEX_STAGE, vertexPath);
	shader.AddStage(FRAGMENT_STAGE, fragmentPath);

	shader.Compile();

	GetUniforms();

}

void PointLightRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	shader.Bind();

	vertexArray->Bind();

	diffuseTexture->SetValue(0);
	normalTexture->SetValue(1);
	materialTexture->SetValue(2);
	depthTexture->SetValue(3);
	shadowCubemap->SetValue(4);

	viewMatrix->SetValue(camera->viewMatrix);
	projectionMatrix->SetValue(camera->projectionMatrix);
	inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

	target->geometryFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);
	target->geometryFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT1)->Bind(GL_TEXTURE1);
	target->geometryFramebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE2);
	target->geometryFramebuffer->GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE3);

	for (auto light : scene->lights) {

		if (light->type != POINT_LIGHT) {
			continue;
		}

		auto pointLight = (PointLight*)light;

		if (pointLight->GetShadow() != nullptr) {
			pointLight->GetShadow()->cubemap->Bind(GL_TEXTURE4);
			lightViewMatrix->SetValue(glm::translate(mat4(1.0f), -pointLight->location) * camera->inverseViewMatrix);
			lightProjectionMatrix->SetValue(pointLight->GetShadow()->components[0].projectionMatrix);
		}

		viewSpaceLightLocation->SetValue(vec3(camera->viewMatrix * vec4(pointLight->location, 1.0f)));
		lightLocation->SetValue(pointLight->location);
		lightColor->SetValue(pointLight->color);
		lightAmbient->SetValue(pointLight->ambient);
		lightRadius->SetValue(pointLight->GetRadius());

		glDrawElements(GL_TRIANGLES, vertexArray->GetIndexComponent()->GetElementCount(),
			vertexArray->GetIndexComponent()->GetDataType(), NULL);

	}

}

void PointLightRenderer::GetUniforms() {

	diffuseTexture = shader.GetUniform("diffuseTexture");
	normalTexture = shader.GetUniform("normalTexture");
	materialTexture = shader.GetUniform("materialTexture");
	depthTexture = shader.GetUniform("depthTexture");
	shadowCubemap = shader.GetUniform("shadowCubemap");
	viewMatrix = shader.GetUniform("vMatrix");
	projectionMatrix = shader.GetUniform("pMatrix");
	inverseProjectionMatrix = shader.GetUniform("ipMatrix");
	lightViewMatrix = shader.GetUniform("lvMatrix");
	lightProjectionMatrix = shader.GetUniform("lpMatrix");
	viewSpaceLightLocation = shader.GetUniform("viewSpaceLightLocation");
	lightLocation = shader.GetUniform("light.location");
	lightColor = shader.GetUniform("light.color");
	lightAmbient = shader.GetUniform("light.ambient");
	lightRadius = shader.GetUniform("light.radius");

}