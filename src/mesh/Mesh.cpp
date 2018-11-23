#include "Mesh.h"
#include "../loader/ModelLoader.h"

Mesh::Mesh(MeshData* data) : data(data) {

	InitializeVertexArray();

}

Mesh::Mesh(string filename) {

	data = ModelLoader::LoadMesh(filename);

	InitializeVertexArray();

}

void Mesh::UpdateData() {

	if (data->indices->ContainsData()) {
		vertexArray->GetIndexComponent()->SetData(data->indices->GetInternal(),
			data->GetIndexCount(), data->indices->GetElementSize());
	}
	if (data->vertices->ContainsData()) {
		vertexArray->GetComponent(0)->SetData(data->vertices->GetInternal(),
			data->GetVertexCount(), data->vertices->GetElementSize());
	}
	if (data->normals->ContainsData()) {
		vertexArray->GetComponent(1)->SetData(data->normals->GetInternal(),
			data->GetVertexCount(), data->normals->GetElementSize());
	}
	if (data->texCoords->ContainsData()) {
		vertexArray->GetComponent(2)->SetData(data->texCoords->GetInternal(),
			data->GetVertexCount(), data->texCoords->GetElementSize());
	}
	if (data->tangents->ContainsData()) {
		vertexArray->GetComponent(3)->SetData(data->tangents->GetInternal(),
			data->GetVertexCount(), data->tangents->GetElementSize());
	}

}

void Mesh::InitializeVertexArray() {

	vertexArray = new VertexArray();

	if (data->indices->ContainsData()) {
		VertexBuffer* indices = new VertexBuffer(GL_ELEMENT_ARRAY_BUFFER, 
			data->indices->GetType(), data->indices->GetStride());
		vertexArray->AddIndexComponent(indices);
	}
	if (data->vertices->ContainsData()) {
		VertexBuffer* vertices = new VertexBuffer(GL_ARRAY_BUFFER,
			data->vertices->GetType(), data->vertices->GetStride());
		vertexArray->AddComponent(0, vertices);
	}
	if (data->normals->ContainsData()) {
		VertexBuffer* normals = new VertexBuffer(GL_ARRAY_BUFFER,
			data->normals->GetType(), data->normals->GetStride());
		vertexArray->AddComponent(1, normals);
	}
	if (data->texCoords->ContainsData()) {
		VertexBuffer* texCoords = new VertexBuffer(GL_ARRAY_BUFFER,
			data->texCoords->GetType(), data->texCoords->GetStride());
		vertexArray->AddComponent(2, texCoords);
	}
	if (data->tangents->ContainsData()) {
		VertexBuffer* tangents = new VertexBuffer(GL_ARRAY_BUFFER,
			data->tangents->GetType(), data->tangents->GetStride());
		vertexArray->AddComponent(3, tangents);;
	}
	
	UpdateData();

}

void Mesh::Bind() {

	vertexArray->Bind();

}

void Mesh::Unbind() {

	vertexArray->Unbind();

}

void Mesh::DeleteContent() {

	vertexArray->DeleteContent();
	delete vertexArray;
	delete data;

	vertexArray = nullptr;

}

Mesh::~Mesh() {

	vertexArray->DeleteContent();
	delete vertexArray;

}