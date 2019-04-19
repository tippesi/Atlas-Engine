#include "Mesh.h"
#include "../loader/ModelLoader.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

namespace Atlas {

	namespace Mesh {

		Mesh::Mesh(MeshData* data, int32_t mobility) : data(data), mobility(mobility) {

			InitializeVertexArray();

			for (auto material : data->materials)
				AddMaterial(material);

		}

		Mesh::Mesh(std::string filename, int32_t mobility) : data(Loader::ModelLoader::LoadMesh(filename)), mobility(mobility) {

			InitializeVertexArray();

			for (auto material : data->materials)
				AddMaterial(material);

		}

		Mesh::~Mesh() {

			

		}

		void Mesh::UpdateData() {

			if (data->indices->ContainsData()) {
				vertexArray.GetIndexComponent()->SetData(data->indices->GetConvertedVoid(),
						0, data->GetIndexCount());
			}
			if (data->vertices->ContainsData()) {
				vertexArray.GetComponent(0)->SetData(data->vertices->GetConvertedVoid(),
						0, data->GetVertexCount());
			}
			if (data->normals->ContainsData()) {
				vertexArray.GetComponent(1)->SetData(data->normals->GetConvertedVoid(),
						0, data->GetVertexCount());
			}
			if (data->texCoords->ContainsData()) {
				vertexArray.GetComponent(2)->SetData(data->texCoords->GetConvertedVoid(),
						0, data->GetVertexCount());
			}
			if (data->tangents->ContainsData()) {
				vertexArray.GetComponent(3)->SetData(data->tangents->GetConvertedVoid(),
						0, data->GetVertexCount());
			}

		}

		void Mesh::Bind() {

			vertexArray.Bind();

		}

		void Mesh::Unbind() {

			vertexArray.Unbind();

		}

		void Mesh::InitializeVertexArray() {

			vertexArray.Unbind();

			if (data->indices->ContainsData()) {
				auto indices = new Buffer::IndexBuffer(data->indices->GetType(),
					data->indices->GetElementSize(), data->GetIndexCount());
				vertexArray.AddIndexComponent(indices);
			}
			if (data->vertices->ContainsData()) {
				auto vertices = new Buffer::VertexBuffer(data->vertices->GetType(), 
					data->vertices->GetStride(), data->vertices->GetElementSize(), data->GetVertexCount());
				vertexArray.AddComponent(0, vertices);
			}
			if (data->normals->ContainsData()) {
				auto normals = new Buffer::VertexBuffer(data->normals->GetType(),
					data->normals->GetStride(), data->normals->GetElementSize(), data->GetVertexCount());
				vertexArray.AddComponent(1, normals);
			}
			if (data->texCoords->ContainsData()) {
				auto texCoords = new Buffer::VertexBuffer(data->texCoords->GetType(),
					data->texCoords->GetStride(), data->texCoords->GetElementSize(), data->GetVertexCount());
				vertexArray.AddComponent(2, texCoords);
			}
			if (data->tangents->ContainsData()) {
				auto tangents = new Buffer::VertexBuffer(data->tangents->GetType(),
					data->tangents->GetStride(), data->tangents->GetElementSize(), data->GetVertexCount());
				vertexArray.AddComponent(3, tangents);
			}

			UpdateData();

		}

		void Mesh::AddMaterial(Material *material) {

			MaterialConfig materialConfig;

			if (material->HasDiffuseMap()) {
				materialConfig.opaqueConfig.AddMacro("DIFFUSE_MAP");
				if (material->diffuseMap->channels == 4)
					materialConfig.shadowConfig.AddMacro("ALPHA");
			}

			if (material->HasNormalMap()) {
				materialConfig.opaqueConfig.AddMacro("NORMAL_MAP");
			}

			configs[material] = materialConfig;

			Renderer::OpaqueRenderer::AddConfig(&configs[material].opaqueConfig);
			Renderer::ShadowRenderer::AddConfig(&configs[material].shadowConfig);

		}

	}

}