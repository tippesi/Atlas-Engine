#include "ShadowRenderer.h"

ShaderBatch* ShadowRenderer::shaderBatch;

ShadowRenderer::ShadowRenderer(const char* vertexSource, const char* fragmentSource) {

	diffuseMapUniform = shaderBatch->GetUniform("diffuseMap");
	modelMatrixUniform = shaderBatch->GetUniform("mMatrix");
	lightSpaceMatrixUniform = shaderBatch->GetUniform("lightSpaceMatrix");

}


void ShadowRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	for (auto light : scene->lights) {

		if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr) {
			continue;
		}

		Framebuffer* framebuffer = light->shadow->cascades[0].map;

		framebuffer->Bind();

		glViewport(0, 0, framebuffer->width, framebuffer->height);

		glClear(GL_DEPTH_BUFFER_BIT);

		for (auto shaderConfigBatch : shaderBatch->configBatches) {

			shaderBatch->Bind(shaderConfigBatch->ID);

			diffuseMapUniform->SetValue(0);

			mat4 lightSpace = light->shadow->cascades[0].projectionMatrix * light->shadow->cascades[0].viewMatrix;

			lightSpaceMatrixUniform->SetValue(lightSpace);

			for (auto actorBatch : scene->actorBatches) {

				Mesh* mesh = actorBatch->GetMesh();
				mesh->Bind();

				for (auto subData : mesh->data->subData) {

					Material* material = mesh->data->materials[subData->materialIndex];

					if (material->shadowConfig->batchID != shaderConfigBatch->ID) {
						continue;
					}

					if (material->HasDiffuseMap())
						material->diffuseMap->Bind(GL_TEXTURE0);

					for (auto actor : actorBatch->actors) {

						if (!actor->render) {
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