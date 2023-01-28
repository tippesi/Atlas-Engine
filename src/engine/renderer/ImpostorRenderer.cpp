#include "ImpostorRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		ImpostorRenderer::ImpostorRenderer() {

            /*
			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			shaderBatch.AddStage(AE_VERTEX_STAGE, "impostor/impostor.vsh");
			shaderBatch.AddStage(AE_FRAGMENT_STAGE, "impostor/impostor.fsh");

			interpolationConfig.AddMacro("INTERPOLATION");

			shaderBatch.AddConfig(&normalConfig);
			shaderBatch.AddConfig(&interpolationConfig);

			GetUniforms();
             */

		}

		void ImpostorRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
			RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap) {

            /*
			Profiler::BeginQuery("Impostors");

			glDisable(GL_CULL_FACE);

			vertexArray.Bind();

			for (uint8_t i = 0; i < 2; i++) {

				switch (i) {
				case 0: shaderBatch.Bind(&normalConfig); break;
				case 1: shaderBatch.Bind(&interpolationConfig); break;
				default: break;
				}

				cameraRight->SetValue(camera->right);
				cameraUp->SetValue(camera->up);

				vMatrix->SetValue(camera->viewMatrix);
				pMatrix->SetValue(camera->projectionMatrix);
				cameraLocation->SetValue(camera->GetLocation());

				pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
				jitterLast->SetValue(camera->GetLastJitter());
				jitterCurrent->SetValue(camera->GetJitter());

				for (auto& key : renderList->impostorBuffers) {

					auto mesh = key.first;
					auto buffer = key.second;

					// If there aren't any impostors there won't be a buffer
					if (!buffer)
						continue;

					if (!mesh->impostor->interpolation)
						continue;

					auto actorCount = buffer->GetElementCount();

					mesh->impostor->baseColorTexture.Bind(0);
					mesh->impostor->roughnessMetalnessAoTexture.Bind(1);
					mesh->impostor->normalTexture.Bind(2);

					// Base 0 is used by the materials
					mesh->impostor->viewPlaneBuffer.BindBase(1);
					buffer->BindBase(2);

					center->SetValue(mesh->impostor->center);
					radius->SetValue(mesh->impostor->radius);

					views->SetValue(mesh->impostor->views);
					cutoff->SetValue(mesh->impostor->cutoff);
					materialIdx->SetValue((uint32_t)materialMap[mesh->impostor]);

					glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)actorCount);

				}

			}

			glEnable(GL_CULL_FACE);

			Profiler::EndQuery();
             */

		}

		void ImpostorRenderer::GetUniforms() {

            /*
			pMatrix = shaderBatch.GetUniform("pMatrix");
			vMatrix = shaderBatch.GetUniform("vMatrix");
			cameraLocation = shaderBatch.GetUniform("cameraLocation");

			center = shaderBatch.GetUniform("center");
			radius = shaderBatch.GetUniform("radius");

			cameraRight = shaderBatch.GetUniform("cameraRight");
			cameraUp = shaderBatch.GetUniform("cameraUp");

			views = shaderBatch.GetUniform("views");
			cutoff = shaderBatch.GetUniform("cutoff");
			materialIdx = shaderBatch.GetUniform("materialIdx");

			pvMatrixLast = shaderBatch.GetUniform("pvMatrixLast");
			jitterLast = shaderBatch.GetUniform("jitterLast");
			jitterCurrent = shaderBatch.GetUniform("jitterCurrent");
             */

		}

	}

}