#include "PointLightRenderer.h"
#include "helper/GeometryHelper.h"
#include "../lighting/PointLight.h"

namespace Atlas {

	namespace Renderer {

		PointLightRenderer::PointLightRenderer() {

			Helper::GeometryHelper::GenerateSphereVertexArray(vertexArray, 16, 16);

			shader.AddStage(AE_VERTEX_STAGE, "deferred/point.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "deferred/point.fsh");

			shader.Compile();

			GetUniforms();

		}

		void PointLightRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			shader.Bind();

			vertexArray.Bind();

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);
			inverseProjectionMatrix->SetValue(camera->invProjectionMatrix);

			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(GL_TEXTURE0);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT1)->Bind(GL_TEXTURE1);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT2)->Bind(GL_TEXTURE2);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT3)->Bind(GL_TEXTURE3);
			target->geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT4)->Bind(GL_TEXTURE4);
			target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE5);

			auto lights = scene->GetLights();

			for (auto light : lights) {

				if (light->type != AE_POINT_LIGHT) {
					continue;
				}

				auto pointLight = (Lighting::PointLight*)light;

				if (pointLight->GetShadow()) {
					pointLight->GetShadow()->cubemap.Bind(GL_TEXTURE6);
					lightViewMatrix->SetValue(glm::translate(mat4(1.0f), -pointLight->location) * camera->invViewMatrix);
					lightProjectionMatrix->SetValue(pointLight->GetShadow()->components[0].projectionMatrix);
					shadowEnabled->SetValue(true);
				}
				else {
					shadowEnabled->SetValue(false);
				}

				viewSpaceLightLocation->SetValue(vec3(camera->viewMatrix * vec4(pointLight->location, 1.0f)));
				lightLocation->SetValue(pointLight->location);
				lightColor->SetValue(pointLight->color);
				//lightAmbient->SetValue(pointLight->ambient);
				lightRadius->SetValue(pointLight->radius);

				glDrawElements(GL_TRIANGLES, (int32_t)vertexArray.GetIndexComponent()->GetElementCount(),
							   vertexArray.GetIndexComponent()->GetDataType(), nullptr);

				if (pointLight->GetShadow()) {
					pointLight->GetShadow()->cubemap.Unbind();
				}

			}

		}

		void PointLightRenderer::GetUniforms() {

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
			shadowEnabled = shader.GetUniform("shadowEnabled");

		}

	}

}