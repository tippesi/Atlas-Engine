#include "ShadowRenderer.h"

string ShadowRenderer::vertexPath = "shadowmapping.vsh";
string ShadowRenderer::fragmentPath = "shadowmapping.fsh";

ShaderBatch ShadowRenderer::shaderBatch;
mutex ShadowRenderer::shaderBatchMutex;

ShadowRenderer::ShadowRenderer() {

	framebuffer = new Framebuffer(0, 0);

	arrayMapUniform = shaderBatch.GetUniform("arrayMap");
	diffuseMapUniform = shaderBatch.GetUniform("diffuseMap");
	modelMatrixUniform = shaderBatch.GetUniform("mMatrix");
	lightSpaceMatrixUniform = shaderBatch.GetUniform("lightSpaceMatrix");

}


void ShadowRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	lock_guard<mutex> guard(shaderBatchMutex);

	bool backFaceCulling = true;

	framebuffer->Bind();

	for (auto& light : scene->lights) {

		if (light->GetShadow() == nullptr) {
			continue;
		}

		if (!light->GetShadow()->update) {
			continue;
		}

		light->GetShadow()->update = false;

		// We expect every cascade to have the same resoltion
		glViewport(0, 0, light->GetShadow()->resolution, light->GetShadow()->resolution);

		for (int32_t i = 0; i < light->GetShadow()->componentCount; i++) {

			ShadowComponent* component = &light->GetShadow()->components[i];

			if (light->GetShadow()->useCubemap) {
				framebuffer->AddComponentCubemap(GL_DEPTH_ATTACHMENT, light->GetShadow()->cubemap, i);
			}
			else {
				framebuffer->AddComponentTextureArray(GL_DEPTH_ATTACHMENT, light->GetShadow()->maps, i);
			}

			glClear(GL_DEPTH_BUFFER_BIT);

			for (auto shaderConfigBatch : shaderBatch.configBatches) {

				shaderBatch.Bind(shaderConfigBatch->ID);

				arrayMapUniform->SetValue(0);
				diffuseMapUniform->SetValue(0);

				mat4 lightSpace = component->projectionMatrix * component->viewMatrix;

				lightSpaceMatrixUniform->SetValue(lightSpace);

				for (auto actorBatch : scene->actorBatches) {

					Mesh* mesh = actorBatch->GetMesh();
					mesh->Bind();

					if (!mesh->cullBackFaces && backFaceCulling) {
						glDisable(GL_CULL_FACE);
						backFaceCulling = false;
					}
					else if (mesh->cullBackFaces && !backFaceCulling) {
						glEnable(GL_CULL_FACE);
						backFaceCulling = true;
					}

					for (auto subData : mesh->data->subData) {

						Material* material = mesh->data->materials[subData->materialIndex];

						if (material->shadowConfig.configBatchID != shaderConfigBatch->ID) {
							continue;
						}

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

						for (auto actor : actorBatch->actors) {

							if (!actor->castShadow) {
								continue;
							}

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

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch.AddStage(VERTEX_STAGE, vertexPath);
	shaderBatch.AddStage(FRAGMENT_STAGE, fragmentPath);

}

void ShadowRenderer::AddConfig(ShaderConfig* config) {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch.AddConfig(config);

}

void ShadowRenderer::RemoveConfig(ShaderConfig* config) {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch.RemoveConfig(config);

}