#include "ShadowRenderer.h"

namespace Atlas {

	namespace Renderer {

		std::string ShadowRenderer::vertexPath = "shadowmapping.vsh";
		std::string ShadowRenderer::fragmentPath = "shadowmapping.fsh";

		Shader::ShaderBatch ShadowRenderer::shaderBatch;
		std::mutex ShadowRenderer::shaderBatchMutex;

		ShadowRenderer::ShadowRenderer() {

			framebuffer = new Framebuffer(0, 0);

			arrayMapUniform = shaderBatch.GetUniform("arrayMap");
			diffuseMapUniform = shaderBatch.GetUniform("diffuseMap");
			modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
			lightSpaceMatrixUniform = shaderBatch.GetUniform("lightSpaceMatrix");

		}


		void ShadowRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			bool backFaceCulling = true;

			framebuffer->Bind();

			for (auto& light : scene->renderList.lights) {

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
						framebuffer->AddComponentCubemap(GL_DEPTH_ATTACHMENT, light->GetShadow()->cubemap, i);
					}
					else {
						framebuffer->AddComponentTextureArray(GL_DEPTH_ATTACHMENT, light->GetShadow()->maps, i);
					}

					glClear(GL_DEPTH_BUFFER_BIT);

					for (auto& renderListBatchesKey : component->renderList->orderedRenderBatches) {

						int32_t configBatchID = renderListBatchesKey.first;
						auto renderListBatches = renderListBatchesKey.second;

						shaderBatch.Bind(configBatchID);

						arrayMapUniform->SetValue(0);
						diffuseMapUniform->SetValue(0);

						mat4 lightSpace = component->projectionMatrix * component->viewMatrix;

						lightSpaceMatrixUniform->SetValue(lightSpace);

						for (auto renderListBatch : renderListBatches) {

							auto meshActorBatch = renderListBatch.meshActorBatch;

							// If there is no actor of that mesh visible we discard it.
							if (meshActorBatch->GetSize() == 0) {
								continue;
							}

							auto mesh = meshActorBatch->GetObject();
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

								auto material = mesh->data->materials[subData->materialIndex];

								if (material->HasDiffuseMap()) {
									if (material->HasArrayMap()) {
										if (material->arrayMap->channels == 4) {
											material->arrayMap->Bind(GL_TEXTURE0);
										}
									}
									else {
										if (material->diffuseMap->channels == 4) {
											material->diffuseMap->Bind(GL_TEXTURE0);
										}
									}
								}

								// We could also use instanced rendering here
								for (auto& actor : meshActorBatch->actors) {

									modelMatrixUniform->SetValue(actor->transformedMatrix);

									glDrawElements(mesh->data->primitiveType, subData->numIndices, mesh->data->indices->GetType(),
												   (void*)(subData->indicesOffset * mesh->data->indices->GetElementSize()));

								}

							}

						}

					}

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

	}

}