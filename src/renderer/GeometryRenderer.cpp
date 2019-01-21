#include "GeometryRenderer.h"

#include <mutex>

string GeometryRenderer::vertexPath = "deferred/geometry.vsh";
string GeometryRenderer::fragmentPath = "deferred/geometry.fsh";

ShaderBatch GeometryRenderer::shaderBatch;
mutex GeometryRenderer::shaderBatchMutex;

GeometryRenderer::GeometryRenderer() {

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

void GeometryRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	lock_guard<mutex> guard(shaderBatchMutex);

	bool backFaceCulling = true;

	for (auto& renderListBatchesKey : scene->renderList->orderedRenderBatches) {

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

			auto mesh = meshActorBatch->GetMesh();
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

				Material* material = mesh->data->materials[subData->materialIndex];

				if (material->HasArrayMap()) {
					material->arrayMap->Bind(GL_TEXTURE0);
					if (material->HasDiffuseMap())
						diffuseMapIndexUniform->SetValue(material->GetDiffuseMapIndex());
					if (material->HasNormalMap())
						normalMapIndexUniform->SetValue(material->GetNormalMapIndex());
					if (material->HasSpecularMap())
						specularMapIndexUniform->SetValue(material->GetSpecularMapIndex());
					if (material->HasDisplacementMap())
						heightMapIndexUniform->SetValue(material->GetDisplacementMapIndex());
				}
				else {
					if (material->HasDiffuseMap())
						material->diffuseMap->Bind(GL_TEXTURE0);
					if (material->HasNormalMap())
						material->normalMap->Bind(GL_TEXTURE1);
					if (material->HasSpecularMap())
						material->specularMap->Bind(GL_TEXTURE2);
					if (material->HasDisplacementMap())
						material->displacementMap->Bind(GL_TEXTURE3);
				}

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

void GeometryRenderer::InitShaderBatch() {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch.AddStage(VERTEX_STAGE, vertexPath);
	shaderBatch.AddStage(FRAGMENT_STAGE, fragmentPath);

}

void GeometryRenderer::AddConfig(ShaderConfig* config) {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch.AddConfig(config);

}

void GeometryRenderer::RemoveConfig(ShaderConfig* config) {

	lock_guard<mutex> guard(shaderBatchMutex);

	shaderBatch.RemoveConfig(config);

}