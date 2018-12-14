#include "GeometryRenderer.h"

#include <mutex>

string GeometryRenderer::vertexPath = "deferred/geometry.vsh";
string GeometryRenderer::fragmentPath = "deferred/geometry.fsh";

ShaderBatch* GeometryRenderer::shaderBatch;
mutex GeometryRenderer::shaderBatchMutex;

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

	bool backFaceCulling = true;

	for (auto& renderListBatchesKey : scene->renderList->orderedRenderBatches) {

		int32_t configBatchID = renderListBatchesKey.first;
		auto renderListBatches = renderListBatchesKey.second;

		shaderBatch->Bind(configBatchID);

		diffuseMapUniform->SetValue(0);
		normalMapUniform->SetValue(1);
		specularMapUniform->SetValue(2);
		heightMapUniform->SetValue(3);

		viewMatrixUniform->SetValue(camera->viewMatrix);
		projectionMatrixUniform->SetValue(camera->projectionMatrix);

		for (auto renderListBatch : renderListBatches) {

			ActorBatch* actorBatch = renderListBatch.actorBatch;

			// If there is no actor of that mesh visible we discard it.
			if (actorBatch->GetSize() == 0) {
				continue;
			}

			auto mesh = actorBatch->GetMesh();
			mesh->Bind();

			if (!mesh->cullBackFaces && backFaceCulling) {
				glDisable(GL_CULL_FACE);
				backFaceCulling = false;
			}
			else if (mesh->cullBackFaces && !backFaceCulling) {
				glEnable(GL_CULL_FACE);
				backFaceCulling = true;
			}

			// Render the sub data of the mesh that use this specific shader
			for (auto& subData : renderListBatch.subData) {

				Material* material = mesh->data->materials[subData->materialIndex];

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
				for (auto& actor : actorBatch->actors) {

					modelMatrixUniform->SetValue(actor->transformedMatrix);

					glDrawElements(mesh->data->primitiveType, subData->numIndices, mesh->data->indices->GetType(),
						(void*)(subData->indicesOffset * mesh->data->indices->GetElementSize()));

				}

			}

		}

	}

}

void GeometryRenderer::InitShaderBatch() {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch = new ShaderBatch();
	shaderBatch->AddComponent(VERTEX_SHADER, vertexPath);
	shaderBatch->AddComponent(FRAGMENT_SHADER, fragmentPath);

}

void GeometryRenderer::AddConfig(ShaderConfig* config) {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch->AddConfig(config);

}

void GeometryRenderer::RemoveConfig(ShaderConfig* config) {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch->RemoveConfig(config);

}