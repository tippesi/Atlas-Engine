#include "ImpostorRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		ImpostorRenderer::ImpostorRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			shader.AddStage(AE_VERTEX_STAGE, "impostor/impostor.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "impostor/impostor.fsh");

			shader.Compile();

			GetUniforms();

			interpolationShader.AddStage(AE_VERTEX_STAGE, "impostor/impostor.vsh");
			interpolationShader.AddStage(AE_FRAGMENT_STAGE, "impostor/impostor.fsh");

			interpolationShader.AddMacro("INTERPOLATION");

			interpolationShader.Compile();

			GetInterpolationUniforms();

		}

		void ImpostorRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
			RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap) {

			glDisable(GL_CULL_FACE);

			vertexArray.Bind();

			shader.Bind();

			vMatrixShader->SetValue(camera->viewMatrix);
			pMatrixShader->SetValue(camera->projectionMatrix);
			cameraLocationShader->SetValue(camera->GetLocation());
			
			pvMatrixLastShader->SetValue(camera->GetLastJitteredMatrix());
			jitterLastShader->SetValue(camera->GetLastJitter());
			jitterCurrentShader->SetValue(camera->GetJitter());

			for (auto& key : renderList->impostorBuffers) {

				auto mesh = key.first;
				auto buffer = key.second;

				// If there aren't any impostors there won't be a buffer
				if (!buffer)
					continue;

				if (mesh->impostor->interpolation)
					continue;

				auto actorCount = buffer->GetElementCount();

				vertexArray.AddInstancedComponent(1, buffer);

				mesh->impostor->baseColorTexture.Bind(GL_TEXTURE0);
				mesh->impostor->roughnessMetalnessAoTexture.Bind(GL_TEXTURE1);
				mesh->impostor->normalTexture.Bind(GL_TEXTURE2);

				// Base 0 is used by the materials
				mesh->impostor->viewPlaneBuffer.BindBase(1);

				centerShader->SetValue(mesh->impostor->center);
				radiusShader->SetValue(mesh->impostor->radius);

				viewsShader->SetValue(mesh->impostor->views);
				cutoffShader->SetValue(mesh->impostor->cutoff);
				materialIdxShader->SetValue((uint32_t)materialMap[mesh->impostor]);

				glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)actorCount);

			}

			interpolationShader.Bind();

			cameraRightInterpolationShader->SetValue(camera->right);
			cameraUpInterpolationShader->SetValue(camera->up);

			vMatrixInterpolationShader->SetValue(camera->viewMatrix);
			pMatrixInterpolationShader->SetValue(camera->projectionMatrix);
			cameraLocationInterpolationShader->SetValue(camera->GetLocation());

			pvMatrixLastInterpolationShader->SetValue(camera->GetLastJitteredMatrix());
			jitterLastInterpolationShader->SetValue(camera->GetLastJitter());
			jitterCurrentInterpolationShader->SetValue(camera->GetJitter());

			for (auto& key : renderList->impostorBuffers) {

				auto mesh = key.first;
				auto buffer = key.second;

				// If there aren't any impostors there won't be a buffer
				if (!buffer)
					continue;

				if (!mesh->impostor->interpolation)
					continue;

				auto actorCount = buffer->GetElementCount();

				vertexArray.AddInstancedComponent(1, buffer);

				mesh->impostor->baseColorTexture.Bind(GL_TEXTURE0);
				mesh->impostor->roughnessMetalnessAoTexture.Bind(GL_TEXTURE1);
				mesh->impostor->normalTexture.Bind(GL_TEXTURE2);

				// Base 0 is used by the materials
				mesh->impostor->viewPlaneBuffer.BindBase(1);

				centerInterpolationShader->SetValue(mesh->impostor->center);
				radiusInterpolationShader->SetValue(mesh->impostor->radius);

				viewsInterpolationShader->SetValue(mesh->impostor->views);
				cutoffInterpolationShader->SetValue(mesh->impostor->cutoff);
				materialIdxInterpolationShader->SetValue((uint32_t)materialMap[mesh->impostor]);

				glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)actorCount);

			}

			glEnable(GL_CULL_FACE);

		}

		void ImpostorRenderer::GetUniforms() {

			pMatrixShader = shader.GetUniform("pMatrix");
			vMatrixShader = shader.GetUniform("vMatrix");
			cameraLocationShader = shader.GetUniform("cameraLocation");

			centerShader = shader.GetUniform("center");
			radiusShader = shader.GetUniform("radius");

			viewsShader = shader.GetUniform("views");
			cutoffShader = shader.GetUniform("cutoff");
			materialIdxShader = shader.GetUniform("materialIdx");

			pvMatrixLastShader = shader.GetUniform("pvMatrixLast");
			jitterLastShader = shader.GetUniform("jitterLast");
			jitterCurrentShader = shader.GetUniform("jitterCurrent");

		}

		void ImpostorRenderer::GetInterpolationUniforms() {

			pMatrixInterpolationShader = interpolationShader.GetUniform("pMatrix");
			vMatrixInterpolationShader = interpolationShader.GetUniform("vMatrix");
			cameraLocationInterpolationShader = interpolationShader.GetUniform("cameraLocation");

			centerInterpolationShader = interpolationShader.GetUniform("center");
			radiusInterpolationShader = interpolationShader.GetUniform("radius");

			cameraRightInterpolationShader = interpolationShader.GetUniform("cameraRight");
			cameraUpInterpolationShader = interpolationShader.GetUniform("cameraUp");

			viewsInterpolationShader = interpolationShader.GetUniform("views");
			cutoffInterpolationShader = interpolationShader.GetUniform("cutoff");
			materialIdxInterpolationShader = interpolationShader.GetUniform("materialIdx");

			pvMatrixLastInterpolationShader = interpolationShader.GetUniform("pvMatrixLast");
			jitterLastInterpolationShader = interpolationShader.GetUniform("jitterLast");
			jitterCurrentInterpolationShader = interpolationShader.GetUniform("jitterCurrent");

		}

	}

}