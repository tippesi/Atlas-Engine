#include "Mesh.h"
#include "../loader/ModelLoader.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

namespace Atlas {

	namespace Mesh {

		Mesh::Mesh(const Mesh& that) {

			DeepCopy(that);

		}

		Mesh::Mesh(MeshData data, int32_t mobility) : mobility(mobility), data(data) {

			InitializeVertexArray();

			for (auto& material : data.materials)
				AddMaterial(&material);

		}

		Mesh::Mesh(std::string filename, int32_t mobility) : mobility(mobility) {

			Loader::ModelLoader::LoadMesh(filename, data);

			InitializeVertexArray();

			for (auto& material : data.materials)
				AddMaterial(&material);

		}

		Mesh::~Mesh() {

			ClearMaterials();

		}

		Mesh& Mesh::operator=(const Mesh& that) {

			if (this != &that) {

				ClearMaterials();

				DeepCopy(that);

			}

			return *this;

		}

		void Mesh::UpdateData() {

			if (data.indices.ContainsData()) {
				vertexArray.GetIndexComponent()->SetData(data.indices.GetConvertedVoid(),
						0, data.GetIndexCount());
			}
			if (data.vertices.ContainsData()) {
				vertexArray.GetComponent(0)->SetData(data.vertices.GetConvertedVoid(),
						0, data.GetVertexCount());
			}
			if (data.normals.ContainsData()) {
				vertexArray.GetComponent(1)->SetData(data.normals.GetConvertedVoid(),
						0, data.GetVertexCount());
			}
			if (data.texCoords.ContainsData()) {
				vertexArray.GetComponent(2)->SetData(data.texCoords.GetConvertedVoid(),
						0, data.GetVertexCount());
			}
			if (data.tangents.ContainsData()) {
				vertexArray.GetComponent(3)->SetData(data.tangents.GetConvertedVoid(),
						0, data.GetVertexCount());
			}

		}

		void Mesh::Bind() const {

			vertexArray.Bind();

		}

		void Mesh::Unbind() const {

			vertexArray.Unbind();

		}

		Shader::ShaderConfig* Mesh::GetConfig(Material* material, int32_t type) {

			auto key = configs.find(material);

			if (key == configs.end())
				return nullptr;

			auto matConfig = key->second;

			switch (type) {
			case AE_OPAQUE_CONFIG: return &matConfig->opaqueConfig;
			case AE_SHADOW_CONFIG: return &matConfig->shadowConfig;
			default: return nullptr;
			}

		}

		void Mesh::InitializeVertexArray() {

			vertexArray.Bind();

			if (data.indices.ContainsData()) {
				auto indices = new Buffer::IndexBuffer(data.indices.GetType(),
					data.indices.GetElementSize(), data.GetIndexCount());
				vertexArray.AddIndexComponent(indices);
			}
			if (data.vertices.ContainsData()) {
				auto vertices = new Buffer::VertexBuffer(data.vertices.GetType(), 
					data.vertices.GetStride(), data.vertices.GetElementSize(), data.GetVertexCount());
				vertexArray.AddComponent(0, vertices);
			}
			if (data.normals.ContainsData()) {
				auto normals = new Buffer::VertexBuffer(data.normals.GetType(),
					data.normals.GetStride(), data.normals.GetElementSize(), data.GetVertexCount());
				vertexArray.AddComponent(1, normals);
			}
			if (data.texCoords.ContainsData()) {
				auto texCoords = new Buffer::VertexBuffer(data.texCoords.GetType(),
					data.texCoords.GetStride(), data.texCoords.GetElementSize(), data.GetVertexCount());
				vertexArray.AddComponent(2, texCoords);
			}
			if (data.tangents.ContainsData()) {
				auto tangents = new Buffer::VertexBuffer(data.tangents.GetType(),
					data.tangents.GetStride(), data.tangents.GetElementSize(), data.GetVertexCount());
				vertexArray.AddComponent(3, tangents);
			}

			vertexArray.Unbind();

			UpdateData();

		}

		void Mesh::AddMaterial(Material *material) {

			auto materialConfig = new MaterialConfig;

			if (material->HasDiffuseMap()) {
				materialConfig->opaqueConfig.AddMacro("DIFFUSE_MAP");
				if (material->diffuseMap->channels == 4)
					materialConfig->shadowConfig.AddMacro("ALPHA");
			}

			if (material->HasNormalMap()) {
				materialConfig->opaqueConfig.AddMacro("NORMAL_MAP");
			}

			configs[material] = materialConfig;

			Renderer::OpaqueRenderer::AddConfig(&materialConfig->opaqueConfig);
			Renderer::ShadowRenderer::AddConfig(&materialConfig->shadowConfig);

		}

		void Mesh::ClearMaterials() {

			for (auto& materialConfigKey : configs) {
				auto materialConfig = materialConfigKey.second;
				Renderer::OpaqueRenderer::RemoveConfig(&materialConfig->opaqueConfig);
				Renderer::ShadowRenderer::RemoveConfig(&materialConfig->shadowConfig);
				delete materialConfig;
			}

			configs.clear();

		}

		void Mesh::DeepCopy(const Mesh& that) {

			data = that.data;
			mobility = that.mobility;
			cullBackFaces = that.cullBackFaces;
			vertexArray = that.vertexArray;

			for (auto& material : data.materials)
				AddMaterial(&material);

		}

	}

}