#include "GeometryRenderer.h"

ShaderBatch* GeometryRenderer::shaderBatch;

GeometryRenderer::GeometryRenderer() {

	diffuseMapUniform = shaderBatch->GetUniform("diffuseMap");
	specularMapUniform = shaderBatch->GetUniform("specularMap");
	normalMapUniform = shaderBatch->GetUniform("normalMap");
	heightMapUniform = shaderBatch->GetUniform("heightMap");
	modelMatrixUniform = shaderBatch->GetUniform("mMatrix");
	viewMatrixUniform = shaderBatch->GetUniform("vMatrix");
	projectionMatrixUniform = shaderBatch->GetUniform("pMatrix");
	diffuseColorUniform = shaderBatch->GetUniform("diffuseColor");
	specularColorUniform = shaderBatch->GetUniform("specularColor");
	ambientColorUniform = shaderBatch->GetUniform("ambientColor");
	specularHardnessUniform = shaderBatch->GetUniform("specularHardness");
	specularIntensityUniform = shaderBatch->GetUniform("specularIntensity");

}

void GeometryRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	for (auto shaderConfigBatch : shaderBatch->configBatches) {

		shaderBatch->Bind(shaderConfigBatch->ID);

		diffuseMapUniform->SetValue(0);
		normalMapUniform->SetValue(1);
		specularMapUniform->SetValue(2);
		heightMapUniform->SetValue(3);

		viewMatrixUniform->SetValue(camera->viewMatrix);
		projectionMatrixUniform->SetValue(camera->projectionMatrix);

		for (auto actorBatch : scene->actorBatches) {

			Mesh* mesh = actorBatch->GetMesh();
			mesh->Bind();

			for (auto subData : mesh->data->subData) {

				Material* material = mesh->data->materials[subData->materialIndex];

				if (material->geometryConfig->batchID != shaderConfigBatch->ID) {
					continue;
				}

				if (material->HasDiffuseMap())
					material->diffuseMap->Bind(GL_TEXTURE0);
				if (material->HasNormalMap())
					material->normalMap->Bind(GL_TEXTURE1);
				if (material->HasSpecularMap())
					material->specularMap->Bind(GL_TEXTURE2);
				if (material->HasHeightMap())
					material->heightMap->Bind(GL_TEXTURE3);

				diffuseColorUniform->SetValue(material->diffuseColor);
				specularColorUniform->SetValue(material->specularColor);
				ambientColorUniform->SetValue(material->ambientColor);
				specularHardnessUniform->SetValue(material->specularHardness);
				specularIntensityUniform->SetValue(material->specularIntensity);

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

void GeometryRenderer::InitShaderBatch(string vertexSource, string fragmentSource) {

	shaderBatch = new ShaderBatch();
	shaderBatch->AddComponent(VERTEX_SHADER, vertexSource);
	shaderBatch->AddComponent(FRAGMENT_SHADER, fragmentSource);

}

void GeometryRenderer::AddConfig(ShaderConfig* config) {

	shaderBatch->AddConfig(config);

}

void GeometryRenderer::RemoveConfig(ShaderConfig* config) {

	shaderBatch->RemoveConfig(config);

}