#include "ShadowRenderer.h"

ShaderBatch* ShadowRenderer::shaderBatch;

ShadowRenderer::ShadowRenderer(const char* vertexSource, const char* fragmentSource) {

	framebuffer = new Framebuffer(0, 0);

	diffuseMapUniform = shaderBatch->GetUniform("diffuseMap");
	modelMatrixUniform = shaderBatch->GetUniform("mMatrix");
	lightSpaceMatrixUniform = shaderBatch->GetUniform("lightSpaceMatrix");

}


void ShadowRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	framebuffer->Bind();

	for (auto light : scene->lights) {

		if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr) {
			continue;
		}

		// We expect every cascade to have the same resoltion
		glViewport(0, 0, light->shadow->resolution, light->shadow->resolution);

		for (int32_t i = 0; i < light->shadow->componentCount; i++) {

			ShadowComponent* component = &light->shadow->components[i];

			framebuffer->AddComponentLayer(GL_DEPTH_ATTACHMENT, light->shadow->maps, i);

			glClear(GL_DEPTH_BUFFER_BIT);

			for (auto shaderConfigBatch : shaderBatch->configBatches) {

				shaderBatch->Bind(shaderConfigBatch->ID);

				diffuseMapUniform->SetValue(0);

				mat4 lightSpace = component->projectionMatrix * component->viewMatrix;

				lightSpaceMatrixUniform->SetValue(lightSpace);

				for (auto actorBatch : scene->actorBatches) {

					Mesh* mesh = actorBatch->GetMesh();
					mesh->Bind();

					for (auto subData : mesh->data->subData) {

						Material* material = mesh->data->materials[subData->materialIndex];

						if (material->shadowConfig->batchID != shaderConfigBatch->ID) {
							continue;
						}


						if (material->HasDiffuseMap()) {
							if (material->diffuseMap->channels == 4) {
								material->diffuseMap->Bind(GL_TEXTURE0);
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

void ShadowRenderer::InitShaderBatch(const char* vertexSource, const char* fragmentSource) {

	shaderBatch = new ShaderBatch();
	shaderBatch->AddComponent(VERTEX_SHADER, vertexSource);
	shaderBatch->AddComponent(FRAGMENT_SHADER, fragmentSource);

}

void ShadowRenderer::AddConfig(ShaderConfig* config) {

	shaderBatch->AddConfig(config);

}

void ShadowRenderer::RemoveConfig(ShaderConfig* config) {

	shaderBatch->RemoveConfig(config);

}