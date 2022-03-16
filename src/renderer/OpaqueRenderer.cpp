#include "OpaqueRenderer.h"

#include "../Clock.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		Shader::ShaderBatch OpaqueRenderer::shaderBatch;
		std::mutex OpaqueRenderer::shaderBatchMutex;

		OpaqueRenderer::OpaqueRenderer() {

			renderList = RenderList(AE_OPAQUE_CONFIG);

			modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
			viewMatrixUniform = shaderBatch.GetUniform("vMatrix");
			projectionMatrixUniform = shaderBatch.GetUniform("pMatrix");

			cameraLocationUniform = shaderBatch.GetUniform("cameraLocation");
			baseColorUniform = shaderBatch.GetUniform("baseColor");
			roughnessUniform = shaderBatch.GetUniform("roughness");
			metalnessUniform = shaderBatch.GetUniform("metalness");
			aoUniform = shaderBatch.GetUniform("ao");
			normalScaleUniform = shaderBatch.GetUniform("normalScale");
			displacementScaleUniform = shaderBatch.GetUniform("displacementScale");

			materialIdxUniform = shaderBatch.GetUniform("materialIdx");

			timeUniform = shaderBatch.GetUniform("time");
			deltaTimeUniform = shaderBatch.GetUniform("deltaTime");

			vegetationUniform = shaderBatch.GetUniform("vegetation");
			invertUVsUniform = shaderBatch.GetUniform("invertUVs");
			staticMeshUniform = shaderBatch.GetUniform("staticMesh");
			twoSidedUniform = shaderBatch.GetUniform("twoSided");

			pvMatrixLast = shaderBatch.GetUniform("pvMatrixLast");

			jitterLast = shaderBatch.GetUniform("jitterLast");
			jitterCurrent = shaderBatch.GetUniform("jitterCurrent");

		}

		void OpaqueRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
			Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap) {

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
					auto key = renderList.actorBatchBuffers.find(mesh);

					if (key == renderList.actorBatchBuffers.end())
						continue;

					auto buffers = key->second;

					if (!buffers.currentMatrices)
						continue;

					auto actorCount = buffers.currentMatrices->GetElementCount();

					if (!actorCount) {
						continue;
					}

					auto staticMesh = mesh->mobility == AE_STATIONARY_MESH;

					mesh->Bind();
					buffers.currentMatrices->BindBase(2);
					if (!staticMesh) buffers.lastMatrices->BindBase(3);

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

					timeUniform->SetValue(Clock::Get());
					deltaTimeUniform->SetValue(Clock::GetDelta());

					staticMeshUniform->SetValue(staticMesh);
					vegetationUniform->SetValue(mesh->vegetation);
					invertUVsUniform->SetValue(mesh->invertUVs);

					// Prepare uniform buffer here
					// Generate all drawing commands
					// We could also batch several materials together because they share the same shader

					// Render the sub data of the mesh that use this specific shader
					for (auto& subData : renderListBatch.subData) {

						auto material = subData->material;

						AdjustFaceCulling(!material->twoSided && mesh->cullBackFaces,
							backFaceCulling);

						if (material->HasBaseColorMap())
							material->baseColorMap->Bind(GL_TEXTURE0);
						if (material->HasOpacityMap())
							material->opacityMap->Bind(GL_TEXTURE1);
						if (material->HasNormalMap())
							material->normalMap->Bind(GL_TEXTURE2);
						if (material->HasRoughnessMap())
							material->roughnessMap->Bind(GL_TEXTURE3);
						if (material->HasMetalnessMap())
							material->metalnessMap->Bind(GL_TEXTURE4);
						if (material->HasAoMap())
							material->aoMap->Bind(GL_TEXTURE5);
						if (material->HasDisplacementMap())
							material->displacementMap->Bind(GL_TEXTURE6);

						cameraLocationUniform->SetValue(camera->GetLocation());
						normalScaleUniform->SetValue(material->normalScale);
						displacementScaleUniform->SetValue(material->displacementScale);

						twoSidedUniform->SetValue(material->twoSided);
						materialIdxUniform->SetValue((uint32_t)materialMap[material]);

						glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
							(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), GLsizei(actorCount));

					}

				}

			}

			glEnable(GL_CULL_FACE);
			glDepthRangef(0.0f, 1.0f);

			impostorRenderer.Render(viewport, target, camera, &renderList, materialMap);

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

			// Iterate through shaders and add macro
			for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {
				auto shaderID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				auto shader = shaderBatch.GetShader(shaderID);
				// We want normals in world space
				// And a specular map no matter what
				shader->AddMacro("GENERATE_IMPOSTOR");
				shader->Compile();
			}

			for (size_t i = 0; i < viewMatrices.size(); i++) {

				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);		

				for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {

					auto shaderID = renderListBatchesKey.first;
					auto renderListBatches = renderListBatchesKey.second;

					auto shader = shaderBatch.GetShader(shaderID);

					shaderBatch.Bind(shaderID);

					projectionMatrixUniform->SetValue(projectionMatrix);
					viewMatrixUniform->SetValue(viewMatrices[i]);

					for (auto renderListBatch : renderListBatches) {

						auto actorBatch = renderListBatch.actorBatch;

						// If there is no actor of that mesh visible we discard it.
						if (!actorBatch->GetSize()) {
							continue;
						}

						auto mesh = actorBatch->GetObject();
						auto key = renderList.actorBatchBuffers.find(mesh);

						if (key == renderList.actorBatchBuffers.end())
							continue;

						auto buffers = key->second;

						mesh->Bind();
						buffers.currentMatrices->BindBase(2);
						if (mesh->mobility != AE_STATIONARY_MESH) buffers.lastMatrices->BindBase(3);

						invertUVsUniform->SetValue(mesh->invertUVs);
						// We *always* want to render impostors two sided
						twoSidedUniform->SetValue(true);

						// Prepare uniform buffer here
						// Generate all drawing commands
						// We could also batch several materials together because they share the same shader

						// Render the sub data of the mesh that use this specific shader
						for (auto& subData : renderListBatch.subData) {

							auto material = subData->material;

							if (material->HasBaseColorMap())
								material->baseColorMap->Bind(GL_TEXTURE0);
							if (material->HasOpacityMap())
								material->opacityMap->Bind(GL_TEXTURE1);
							if (material->HasNormalMap())
								material->normalMap->Bind(GL_TEXTURE2);
							if (material->HasRoughnessMap())
								material->roughnessMap->Bind(GL_TEXTURE3);
							if (material->HasMetalnessMap())
								material->metalnessMap->Bind(GL_TEXTURE4);
							if (material->HasAoMap())
								material->aoMap->Bind(GL_TEXTURE5);
							if (material->HasDisplacementMap())
								material->displacementMap->Bind(GL_TEXTURE6);

							baseColorUniform->SetValue(material->baseColor);
							roughnessUniform->SetValue(material->roughness);
							metalnessUniform->SetValue(material->metalness);
							aoUniform->SetValue(material->ao);
							normalScaleUniform->SetValue(material->normalScale);
							displacementScaleUniform->SetValue(material->displacementScale);

							glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
								(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), actorBatch->GetSize());

						}

					}

				}

				impostor->baseColorTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0),
					0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);
				// Just use the geometry normals for now. We might add support for normal maps later.
				// For now we just assume that the geometry itself has enough detail when viewed from distance
				impostor->normalTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT2),
					0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);
				impostor->roughnessMetalnessAoTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT3),
					0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);

				glFlush();

			}

			// Iterate through shaders and remove macro
			for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {
				auto shaderID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				auto shader = shaderBatch.GetShader(shaderID);
				shader->RemoveMacro("GENERATE_IMPOSTOR");
				shader->Compile();
			}

			renderList.Clear();
			framebuffer->Unbind();

			glEnable(GL_CULL_FACE);

		}

		void OpaqueRenderer::InitShaderBatch() {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddStage(AE_VERTEX_STAGE, "deferred/geometry.vsh");
			shaderBatch.AddStage(AE_FRAGMENT_STAGE, "deferred/geometry.fsh");

		}

		void OpaqueRenderer::AddConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddConfig(config);

		}

		void OpaqueRenderer::RemoveConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.RemoveConfig(config);

		}

		void OpaqueRenderer::AdjustFaceCulling(bool cullFaces, bool& state) {

			if (!cullFaces && state) {
				glDisable(GL_CULL_FACE);
				state = false;
			}
			else if (cullFaces && !state) {
				glEnable(GL_CULL_FACE);
				state = true;
			}

		}

	}

}