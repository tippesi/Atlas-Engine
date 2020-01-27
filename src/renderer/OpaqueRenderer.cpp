#include "OpaqueRenderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		std::string OpaqueRenderer::vertexPath = "deferred/geometry.vsh";
		std::string OpaqueRenderer::fragmentPath = "deferred/geometry.fsh";

		Shader::ShaderBatch OpaqueRenderer::shaderBatch;
		std::mutex OpaqueRenderer::shaderBatchMutex;

		OpaqueRenderer::OpaqueRenderer() {

			renderList = RenderList(AE_OPAQUE_CONFIG);

			modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
			viewMatrixUniform = shaderBatch.GetUniform("vMatrix");
			projectionMatrixUniform = shaderBatch.GetUniform("pMatrix");

			cameraLocationUniform = shaderBatch.GetUniform("cameraLocation");
			diffuseColorUniform = shaderBatch.GetUniform("diffuseColor");
			emissiveColorUniform = shaderBatch.GetUniform("emissiveColor");
			specularColorUniform = shaderBatch.GetUniform("specularColor");
			ambientColorUniform = shaderBatch.GetUniform("ambientColor");
			specularHardnessUniform = shaderBatch.GetUniform("specularHardness");
			specularIntensityUniform = shaderBatch.GetUniform("specularIntensity");
			normalScaleUniform = shaderBatch.GetUniform("normalScale");
			displacementScaleUniform = shaderBatch.GetUniform("displacementScale");

			invertUVsUniform = shaderBatch.GetUniform("invertUVs");

			pvMatrixLast = shaderBatch.GetUniform("pvMatrixLast");

			jitterLast = shaderBatch.GetUniform("jitterLast");
			jitterCurrent = shaderBatch.GetUniform("jitterCurrent");

		}

		void OpaqueRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			bool backFaceCulling = true;
			bool depthTest = true;

			scene->GetRenderList(camera->frustum, renderList);

			renderList.UpdateBuffers(camera);

			for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {

				auto shaderID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				shaderBatch.Bind(shaderID);

				viewMatrixUniform->SetValue(camera->viewMatrix);
				projectionMatrixUniform->SetValue(camera->projectionMatrix);

				jitterLast->SetValue(camera->GetLastJitter());
				jitterCurrent->SetValue(camera->GetJitter());

				pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());			

				for (auto renderListBatch : renderListBatches) {

					auto actorBatch = renderListBatch.actorBatch;

					// If there is no actor of that mesh visible we discard it.
					if (!actorBatch->GetSize()) {
						continue;
					}

					auto mesh = actorBatch->GetObject();
					auto buffer = renderList.actorBatchBuffers[mesh];

					if (!buffer)
						continue;

					auto actorCount = buffer->GetElementCount();

					if (!actorCount) {
						continue;
					}

					mesh->Bind();

					if (!mesh->cullBackFaces && backFaceCulling) {
						glDisable(GL_CULL_FACE);
						backFaceCulling = false;
					}
					else if (mesh->cullBackFaces && !backFaceCulling) {
						glEnable(GL_CULL_FACE);
						backFaceCulling = true;
					}

					if (!mesh->depthTest && depthTest) {
						// Allows for most objects to have
						// depth test in themselves but are always
						// drawn no matter if something else is nearer
						glDepthRangef(0.0f, 0.001f);
						depthTest = false;
					}
					else if (mesh->depthTest && !depthTest) {
						glDepthRangef(0.0f, 1.0f);
						depthTest = true;
					}

					invertUVsUniform->SetValue(mesh->invertUVs);

					// Prepare uniform buffer here
					// Generate all drawing commands
					// We could also batch several materials together because they share the same shader

					// Render the sub data of the mesh that use this specific shader
					for (auto& subData : renderListBatch.subData) {

						auto material = subData->material;

						if (material->HasDiffuseMap())
							material->diffuseMap->Bind(GL_TEXTURE0);
						if (material->HasNormalMap())
							material->normalMap->Bind(GL_TEXTURE1);
						if (material->HasSpecularMap())
							material->specularMap->Bind(GL_TEXTURE2);
						if (material->HasDisplacementMap())
							material->displacementMap->Bind(GL_TEXTURE3);

						cameraLocationUniform->SetValue(camera->GetLocation());
						diffuseColorUniform->SetValue(material->diffuseColor);
						emissiveColorUniform->SetValue(material->emissiveColor);
						specularColorUniform->SetValue(material->specularColor);
						ambientColorUniform->SetValue(material->ambientColor);
						specularHardnessUniform->SetValue(material->specularHardness);
						specularIntensityUniform->SetValue(material->specularIntensity);
						normalScaleUniform->SetValue(material->normalScale);
						displacementScaleUniform->SetValue(material->displacementScale);

						glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
							(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), actorCount);

					}

				}

			}

			glEnable(GL_CULL_FACE);
			glDepthRangef(0.0f, 1.0f);

			impostorRenderer.Render(viewport, target, camera, &renderList);

			renderList.Clear();

		}

		void OpaqueRenderer::RenderImpostor(Viewport* viewport, Framebuffer* framebuffer, std::vector<mat4> viewMatrices,
			mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor) {

			if (!viewMatrices.size())
				return;

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			Actor::MovableMeshActor actor(mesh);

			framebuffer->Bind(true);

			glDisable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			Camera camera;

			camera.viewMatrix = viewMatrices[0];
			camera.projectionMatrix = projectionMatrix;

			renderList.Add(&actor);
			renderList.UpdateBuffers(&camera);

			for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {

				auto shaderID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				auto shader = shaderBatch.GetShader(shaderID);

				// We want normals in world space
				shader->AddMacro("WORLD_TRANSFORM");

				shader->Compile();
				shader->Bind();

				projectionMatrixUniform->SetValue(projectionMatrix);

				for (size_t i = 0; i < viewMatrices.size(); i++) {

					glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
					glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

					viewMatrixUniform->SetValue(viewMatrices[i]);

					for (auto renderListBatch : renderListBatches) {

						auto actorBatch = renderListBatch.actorBatch;

						// If there is no actor of that mesh visible we discard it.
						if (!actorBatch->GetSize()) {
							continue;
						}

						auto mesh = actorBatch->GetObject();
						mesh->Bind();

						invertUVsUniform->SetValue(mesh->invertUVs);

						// Prepare uniform buffer here
						// Generate all drawing commands
						// We could also batch several materials together because they share the same shader

						// Render the sub data of the mesh that use this specific shader
						for (auto& subData : renderListBatch.subData) {

							auto material = subData->material;

							if (material->HasDiffuseMap())
								material->diffuseMap->Bind(GL_TEXTURE0);
							if (material->HasNormalMap())
								material->normalMap->Bind(GL_TEXTURE1);
							if (material->HasSpecularMap())
								material->specularMap->Bind(GL_TEXTURE2);
							if (material->HasDisplacementMap())
								material->displacementMap->Bind(GL_TEXTURE3);

							//cameraLocationUniform->SetValue(vec3(viewMatrices));
							diffuseColorUniform->SetValue(material->diffuseColor);
							specularColorUniform->SetValue(material->specularColor);
							ambientColorUniform->SetValue(material->ambientColor);
							specularHardnessUniform->SetValue(material->specularHardness);
							specularIntensityUniform->SetValue(material->specularIntensity);
							displacementScaleUniform->SetValue(material->displacementScale);

							glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
								(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), actorBatch->GetSize());

						}

					}

					impostor->diffuseTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0),
						0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);
					impostor->normalTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT1),
						0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);
					impostor->specularTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT2),
						0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);

				}

				// Remove macro again and recompile
				shader->RemoveMacro("WORLD_TRANSFORM");
				shader->Compile();

			}

			renderList.Clear();
			framebuffer->Unbind();

			glEnable(GL_CULL_FACE);

		}

		void OpaqueRenderer::InitShaderBatch() {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddStage(AE_VERTEX_STAGE, vertexPath);
			shaderBatch.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

		}

		void OpaqueRenderer::AddConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddConfig(config);

		}

		void OpaqueRenderer::RemoveConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.RemoveConfig(config);

		}

	}

}