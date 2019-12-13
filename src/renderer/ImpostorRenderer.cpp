#include "ImpostorRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string ImpostorRenderer::vertexPath = "impostor.vsh";
		std::string ImpostorRenderer::fragmentPath = "impostor.fsh";

		ImpostorRenderer::ImpostorRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void ImpostorRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, RenderList* renderList) {

			shader.Bind();

			vertexArray.Bind();

			vMatrix->SetValue(camera->viewMatrix);
			pMatrix->SetValue(camera->projectionMatrix);
			
			pvMatrixLast->SetValue(pvMatrixPrev);
			jitterLast->SetValue(jitterPrev);
			jitterCurrent->SetValue(camera->GetJitter());

			right->SetValue(camera->right);
			up->SetValue(vec3(0.0f, 1.0f, 0.0f));

			for (auto& key : renderList->impostorBuffers) {

				auto mesh = key.first;
				auto buffer = key.second;

				auto actorCount = buffer->GetElementCount();

				if (!actorCount)
					continue;

				vertexArray.AddInstancedComponent(1, buffer);

				mesh->impostor->diffuseTexture.Bind(GL_TEXTURE0);
				mesh->impostor->normalTexture.Bind(GL_TEXTURE1);

				min->SetValue(mesh->impostor->aabb.min);
				max->SetValue(mesh->impostor->aabb.max);

				glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, actorCount);

			}

			pvMatrixPrev = camera->projectionMatrix * camera->viewMatrix;
			jitterPrev = camera->GetJitter();

		}

		void ImpostorRenderer::GetUniforms() {

			pMatrix = shader.GetUniform("pMatrix");
			vMatrix = shader.GetUniform("vMatrix");

			right = shader.GetUniform("right");
			up = shader.GetUniform("up");

			min = shader.GetUniform("minVec");
			max = shader.GetUniform("maxVec");

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

		}

	}

}