#include "geometryrenderer.h"

GeometryRenderer::GeometryRenderer() {



}

void GeometryRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	target->geometryFramebuffer->Bind();

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, target->geometryFramebuffer->width, target->geometryFramebuffer->height);

	for (auto actorBatch : scene->actorBatches) {

		Mesh* mesh = actorBatch->GetMesh();		
		mesh->Bind();

		for (auto subData : mesh->data->subData) {

			Material* material = mesh->data->materials[subData->materialIndex];
			
			material->Bind(camera->viewMatrix, camera->projectionMatrix);
			Uniform* modelMatrixUniform = material->GetModelMatrixUniform();

			for (auto actor : actorBatch->actors) {

				modelMatrixUniform->SetValue(actor->transformedMatrix);

				glDrawElements(mesh->data->primitiveType, subData->numIndices, mesh->data->indices->GetType(), 
					(void*)(subData->indicesOffset * mesh->data->indices->GetElementSize()));

			}

		}

	}

	target->geometryFramebuffer->Unbind();

}