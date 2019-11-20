#include "ShadowRenderer.h"

namespace Atlas {

	namespace Renderer {

		std::string ShadowRenderer::vertexPath = "shadowmapping.vsh";
		std::string ShadowRenderer::fragmentPath = "shadowmapping.fsh";

		Shader::ShaderBatch ShadowRenderer::shaderBatch;
		std::mutex ShadowRenderer::shaderBatchMutex;

		ShadowRenderer::ShadowRenderer() {

			renderList = RenderList(AE_SHADOW_CONFIG);

			modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
			lightSpaceMatrixUniform = shaderBatch.GetUniform("lightSpaceMatrix");

		}


		void ShadowRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			bool backFaceCulling = true;

			framebuffer.Bind();

			auto lights = scene->GetLights();

			for (auto& light : lights) {

				if (!light->GetShadow()) {
					continue;
				}

				if (!light->GetShadow()->update) {
					continue;
				}

				light->GetShadow()->update = false;

				// We expect every cascade to have the same resolution
				glViewport(0, 0, light->GetShadow()->resolution, light->GetShadow()->resolution);

				for (int32_t i = 0; i < light->GetShadow()->componentCount; i++) {

					auto component = &light->GetShadow()->components[i];

					if (light->GetShadow()->useCubemap) {
						framebuffer.AddComponentCubemap(GL_DEPTH_ATTACHMENT, &light->GetShadow()->cubemap, i);
					}
					else {
						framebuffer.AddComponentTextureArray(GL_DEPTH_ATTACHMENT, &light->GetShadow()->maps, i);
					}

					glClear(GL_DEPTH_BUFFER_BIT);


					auto inverseMatrix = glm::inverse(component->frustumMatrix);

					auto corners = GetFrustumCorners(inverseMatrix);
					auto frustum = Volume::Frustum(corners);

					scene->GetRenderList(frustum, renderList);
					renderList.UpdateBuffers();

					for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {

						int32_t shaderID = renderListBatchesKey.first;
						auto renderListBatches = renderListBatchesKey.second;

						shaderBatch.Bind(shaderID);

						mat4 lightSpace = component->projectionMatrix * component->viewMatrix;

						lightSpaceMatrixUniform->SetValue(lightSpace);

						for (auto renderListBatch : renderListBatches) {

							auto actorBatch = renderListBatch.actorBatch;

							// If there is no actor of that mesh visible we discard it.
							if (!actorBatch->GetSize()) {
								continue;
							}

							auto mesh = actorBatch->GetObject();
							mesh->Bind();

							if (!mesh->cullBackFaces && backFaceCulling) {
								glDisable(GL_CULL_FACE);
								backFaceCulling = false;
							}
							else if (mesh->cullBackFaces && !backFaceCulling) {
								glEnable(GL_CULL_FACE);
								backFaceCulling = true;
							}

							// Prepare uniform buffer here
							// Generate all drawing commands
							// We could also batch several materials together because the share the same shader

							// Render the sub data of the mesh that use this specific shader
							for (auto& subData : renderListBatch.subData) {

								auto material = subData->material;

								if (material->HasDiffuseMap()) {
									if (material->diffuseMap->channels == 4) {
										material->diffuseMap->Bind(GL_TEXTURE0);
									}
								}

								glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
									(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), actorBatch->GetSize());

							}

						}

					}

					renderList.Clear();

				}

			}

		}

		void ShadowRenderer::InitShaderBatch() {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddStage(AE_VERTEX_STAGE, vertexPath);
			shaderBatch.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

		}

		void ShadowRenderer::AddConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddConfig(config);

		}

		void ShadowRenderer::RemoveConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.RemoveConfig(config);

		}

		std::vector<vec3> ShadowRenderer::GetFrustumCorners(mat4 inverseMatrix) {

			vec3 vectors[8] = {
				vec3(-1.0f, 1.0f, 1.0f),
				vec3(1.0f, 1.0f, 1.0f),
				vec3(-1.0f, -1.0f, 1.0f),
				vec3(1.0f, -1.0f, 1.0f),
				vec3(-1.0f, 1.0f, -1.0f),
				vec3(1.0f, 1.0f, -1.0f),
				vec3(-1.0f, -1.0f, -1.0f),
				vec3(1.0f, -1.0f, -1.0f)
			};

			std::vector<vec3> corners;

			for (uint8_t i = 0; i < 8; i++) {
				auto homogenous = inverseMatrix * vec4(vectors[i], 1.0f);
				corners.push_back(vec3(homogenous) / homogenous.w);
			}

			return corners;

		}

	}

}