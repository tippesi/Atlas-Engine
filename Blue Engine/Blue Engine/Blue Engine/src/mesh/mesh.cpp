#include "mesh.h"
#include "../loader/modelloader.h"

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

void Mesh::InitializeVBO() {

	glGenBuffers(1, &indices);
	glGenBuffers(1, &vertices);
	glGenBuffers(1, &texCoords);
	glGenBuffers(1, &normals);
	glGenBuffers(1, &tangents);

	UpdateData();

}

void  Mesh::InitializeVAO() {

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);

	if (data->vertices->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, vertices);
		glVertexAttribPointer(0, data->vertices->GetStride(), data->vertices->GetType(), false, 0, NULL);
	}

	if (data->normals->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, normals);
		glVertexAttribPointer(1, data->normals->GetStride(), data->normals->GetType(), false, 0, NULL);
	}

	if (data->texCoords->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, texCoords);
		glVertexAttribPointer(2, data->texCoords->GetStride(), data->texCoords->GetType(), false, 0, NULL);
	}

	if (data->tangents->ContainsData()) {
		glBindBuffer(GL_ARRAY_BUFFER, tangents);
		glVertexAttribPointer(3, data->tangents->GetStride(), data->tangents->GetType(), false, 0, NULL);
	}

	glBindVertexArray(0);

}

void Mesh::Bind() {

	glBindVertexArray(vao);;

}

void Mesh::Unbind() {

	glBindVertexArray(0);

}

Mesh::~Mesh() {



}