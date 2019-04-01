#include "OpaqueRenderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		std::string OpaqueRenderer::vertexPath = "deferred/geometry.vsh";
		std::string OpaqueRenderer::fragmentPath = "deferred/geometry.fsh";

		Shader::ShaderBatch OpaqueRenderer::shaderBatch;
		std::mutex OpaqueRenderer::shaderBatchMutex;

		OpaqueRenderer::OpaqueRenderer() {

			arrayMapUniform = shaderBatch.GetUniform("arrayMapUniform");
			diffuseMapUniform = shaderBatch.GetUniform("diffuseMap");
			specularMapUniform = shaderBatch.GetUniform("specularMap");
			normalMapUniform = shaderBatch.GetUniform("normalMap");
			heightMapUniform = shaderBatch.GetUniform("heightMap");

			diffuseMapIndexUniform = shaderBatch.GetUniform("diffuseMapIndex");
			normalMapIndexUniform = shaderBatch.GetUniform("normalMapIndex");
			specularMapIndexUniform = shaderBatch.GetUniform("specularMapIndex");
			heightMapIndexUniform = shaderBatch.GetUniform("heightMapIndex");

			modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
			viewMatrixUniform = shaderBatch.GetUniform("vMatrix");
			projectionMatrixUniform = shaderBatch.GetUniform("pMatrix");

			diffuseColorUniform = shaderBatch.GetUniform("diffuseColor");
			specularColorUniform = shaderBatch.GetUniform("specularColor");
			ambientColorUniform = shaderBatch.GetUniform("ambientColor");
			specularHardnessUniform = shaderBatch.GetUniform("specularHardness");
			specularIntensityUniform = shaderBatch.GetUniform("specularIntensity");

		}

		void OpaqueRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			bool backFaceCulling = true;

			for (auto& renderListBatchesKey : scene->renderList.orderedRenderBatches) {

				int32_t configBatchID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				shaderBatch.Bind(configBatchID);

				arrayMapUniform->SetValue(0);
				diffuseMapUniform->SetValue(0);
				normalMapUniform->SetValue(1);
				specularMapUniform->SetValue(2);
				heightMapUniform->SetValue(3);

				viewMatrixUniform->SetValue(camera->viewMatrix);
				projectionMatrixUniform->SetValue(camera->projectionMatrix);

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

						if (material->HasDiffuseMap())
							material->diffuseMap->Bind(GL_TEXTURE0);
						if (material->HasNormalMap())
							material->normalMap->Bind(GL_TEXTURE1);
						if (material->HasSpecularMap())
							material->specularMap->Bind(GL_TEXTURE2);
						if (material->HasDisplacementMap())
							material->displacementMap->Bind(GL_TEXTURE3);

						diffuseColorUniform->SetValue(material->diffuseColor);
						specularColorUniform->SetValue(material->specularColor);
						ambientColorUniform->SetValue(material->ambientColor);
						specularHardnessUniform->SetValue(material->specularHardness);
						specularIntensityUniform->SetValue(material->specularIntensity);

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