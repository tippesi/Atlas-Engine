#include "ShadowRenderer.h"

#include "../Clock.h"

#include "../lighting/DirectionalLight.h"
#include "../lighting/PointLight.h"

namespace Atlas {

	namespace Renderer {

		Shader::ShaderBatch ShadowRenderer::shaderBatch;
		std::mutex ShadowRenderer::shaderBatchMutex;

		ShadowRenderer::ShadowRenderer() {

			renderList = RenderList(AE_SHADOW_CONFIG);

			modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
			lightSpaceMatrixUniform = shaderBatch.GetUniform("lightSpaceMatrix");

			timeUniform = shaderBatch.GetUniform("time");

			vegetationUniform = shaderBatch.GetUniform("vegetation");
			invertUVsUniform = shaderBatch.GetUniform("invertUVs");

		}

		void ShadowRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			Profiler::BeginQuery("Shadows");

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			framebuffer.Bind();

			auto lights = scene->GetLights();

			for (auto& light : lights) {

				if (!light->GetShadow()) {
					continue;
				}

				if (!light->GetShadow()->update) {
					continue;
				}

				// We expect every cascade to have the same resolution
				glViewport(0, 0, light->GetShadow()->resolution, light->GetShadow()->resolution);

				// We don't want to render to the long range component if it exists
				auto componentCount = light->GetShadow()->longRange ? 
					light->GetShadow()->componentCount - 1 : 
					light->GetShadow()->componentCount;

				bool isDirectionalLight = false;
				vec3 lightLocation;

				if (light->type == AE_DIRECTIONAL_LIGHT) {
					auto directionLight = static_cast<Lighting::DirectionalLight*>(light);
					lightLocation = 1000000.0f * -normalize(directionLight->direction);
					isDirectionalLight = true;
				}
				else if (light->type == AE_POINT_LIGHT) {
					auto pointLight = static_cast<Lighting::PointLight*>(light);
					lightLocation = pointLight->location;
				}

				for (int32_t i = 0; i < componentCount; i++) {

					// We need to reset the culling since the impostor renderer can
					// change the culling up
					bool backFaceCulling = true;
					glEnable(GL_CULL_FACE);

					auto component = &light->GetShadow()->components[i];

					if (light->GetShadow()->useCubemap) {
						framebuffer.AddComponentCubemap(GL_DEPTH_ATTACHMENT, &light->GetShadow()->cubemap, i);
					}
					else {
						framebuffer.AddComponentTextureArray(GL_DEPTH_ATTACHMENT, &light->GetShadow()->maps, i);
					}

					auto frustum = Volume::Frustum(component->frustumMatrix);

					scene->GetRenderList(frustum, renderList);
					renderList.UpdateBuffers(camera);

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
							auto key = renderList.actorBatchBuffers.find(mesh);

							if (key == renderList.actorBatchBuffers.end())
								continue;

							auto buffers = key->second;

							if (!buffers.currentMatrices)
								continue;

							auto actorCount = buffers.currentMatrices->GetElementCount();

							mesh->Bind();
							buffers.currentMatrices->BindBase(2);

							timeUniform->SetValue(Clock::Get());

							vegetationUniform->SetValue(mesh->vegetation);
							invertUVsUniform->SetValue(mesh->invertUVs);

							// Prepare uniform buffer here
							// Generate all drawing commands
							// We could also batch several materials together because the share the same shader

							// Render the sub data of the mesh that use this specific shader
							for (auto& subData : renderListBatch.subData) {

								auto material = subData->material;

								AdjustFaceCulling(!material->twoSided && mesh->cullBackFaces,
									backFaceCulling);

								if (material->HasOpacityMap()) {
									material->opacityMap->Bind(GL_TEXTURE0);
								}

								glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
									(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), GLsizei(actorCount));

							}

						}

					}

					impostorRenderer.Render(viewport, target, &renderList, component->viewMatrix,
						component->projectionMatrix, lightLocation);

					renderList.Clear();

				}

			}

			Profiler::EndQuery();

		}

		void ShadowRenderer::InitShaderBatch() {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddStage(AE_VERTEX_STAGE, "shadowmapping.vsh");
			shaderBatch.AddStage(AE_FRAGMENT_STAGE, "shadowmapping.fsh");

		}

		void ShadowRenderer::AddConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.AddConfig(config);

		}

		void ShadowRenderer::RemoveConfig(Shader::ShaderConfig* config) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			shaderBatch.RemoveConfig(config);

		}

		void ShadowRenderer::AdjustFaceCulling(bool cullFaces, bool& state) {

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