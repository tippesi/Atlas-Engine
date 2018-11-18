#include "Mesh.h"
#include "../loader/ModelLoader.h"

Mesh::Mesh(MeshData* data) : data(data) {

	InitializeVBO();
	InitializeVAO();

}

Mesh::Mesh(const char* filename) {

	data = ModelLoader::LoadMesh(filename);

	InitializeVBO();
	InitializeVAO();

}

void Mesh::UpdateData() {

	if (data->indices->ContainsData()) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesVbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)data->GetIndexCount() * data->indices->GetElementSize(),
			data->indices->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->vertices->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, verticesVbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->vertices->GetElementSize(),
			data->vertices->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->texCoords->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, texCoordsVbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->texCoords->GetElementSize(),
			data->texCoords->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->normals->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, normalsVbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->normals->GetElementSize(),
			data->normals->GetInternal(), GL_STATIC_DRAW);
	}

	if (data->tangents->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, tangentsVbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data->GetVertexCount() * data->tangents->GetElementSize(),
			data->tangents->GetInternal(), GL_STATIC_DRAW);
	}

}

void Mesh::InitializeVBO() {

	glGenBuffers(1, &indicesVbo);
	glGenBuffers(1, &verticesVbo);
	glGenBuffers(1, &texCoordsVbo);
	glGenBuffers(1, &normalsVbo);
	glGenBuffers(1, &tangentsVbo);

	UpdateData();

}

void  Mesh::InitializeVAO() {

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesVbo);

	if (data->vertices->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, verticesVbo);
		glVertexAttribPointer(0, data->vertices->GetStride(), data->vertices->GetType(), false, 0, NULL);
	}

	if (data->normals->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, normalsVbo);
		glVertexAttribPointer(1, data->normals->GetStride(), data->normals->GetType(), false, 0, NULL);
	}

	if (data->texCoords->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, texCoordsVbo);
		glVertexAttribPointer(2, data->texCoords->GetStride(), data->texCoords->GetType(), false, 0, NULL);
	}

	if (data->tangents->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, tangentsVbo);
		glVertexAttribPointer(3, data->tangents->GetStride(), data->tangents->GetType(), false, 0, NULL);
	}

	glBindVertexArray(0);

}

void Mesh::Bind() {

	glBindVertexArray(vao);

}

void Mesh::Unbind() {

	glBindVertexArray(0);

}

Mesh::~Mesh() {



}