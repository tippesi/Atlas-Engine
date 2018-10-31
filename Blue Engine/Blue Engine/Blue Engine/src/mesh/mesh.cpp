#include "mesh.h"

Mesh::Mesh(MeshData* data) : data(data) {

	glGenBuffers(1, &indices);
	glGenBuffers(1, &vertices);
	glGenBuffers(1, &texCoords);
	glGenBuffers(1, &normals);
	glGenBuffers(1, &tangents);

	UpdateData();

}

Mesh::Mesh(const char* filename) {



}

void Mesh::UpdateData() {

	if (data->indices->ContainsData()) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)data->GetIndexCount() * data->indices->GetElementSize(),
			data->indices->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->vertices->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, vertices);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->vertices->GetElementSize(),
			data->vertices->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->texCoords->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, texCoords);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->texCoords->GetElementSize(),
			data->texCoords->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->normals->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, normals);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->normals->GetElementSize(),
			data->normals->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->tangents->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, tangents);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->tangents->GetElementSize(),
			data->tangents->GetInternal(), GL_STATIC_DRAW);
	}

}