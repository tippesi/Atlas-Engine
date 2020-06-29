#include "ImpostorShadowRenderer.h"

#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		ImpostorShadowRenderer::ImpostorShadowRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, "impostor/impostorShadow.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "impostor/impostorShadow.fsh");

			shader.Compile();

			GetUniforms();

		}

		void ImpostorShadowRenderer::Render(Viewport* viewport, RenderTarget* target, RenderList* renderList,
			mat4 viewMatrix, mat4 projectionMatrix, vec3 location) {

			shader.Bind();

			vertexArray.Bind();

			vMatrix->SetValue(viewMatrix);
			pMatrix->SetValue(projectionMatrix);
			cameraLocation->SetValue(location);

			glDisable(GL_CULL_FACE);

			for (auto& key : renderList->impostorBuffers) {

				auto mesh = key.first;
				auto buffer = key.second;

				// If there aren't any impostors there won't be a buffer
				if (!buffer)
					continue;

				auto actorCount = buffer->GetElementCount();

				vertexArray.AddInstancedComponent(1, buffer);

				mesh->impostor->baseColorTexture.Bind(GL_TEXTURE0);

				// Base 0 is used by the materials
				mesh->impostor->viewPlaneBuffer.BindBase(1);

				center->SetValue(mesh->impostor->center);
				radius->SetValue(mesh->impostor->radius);

				views->SetValue(mesh->impostor->views);
				cutoff->SetValue(mesh->impostor->cutoff);

				glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)actorCount);

			}

			glEnable(GL_CULL_FACE);

		}

		void ImpostorShadowRenderer::GetUniforms() {

			pMatrix = shader.GetUniform("pMatrix");
			vMatrix = shader.GetUniform("vMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");

			center = shader.GetUniform("center");
			radius = shader.GetUniform("radius");

			views = shader.GetUniform("views");
			cutoff = shader.GetUniform("cutoff");

		}

	}

}