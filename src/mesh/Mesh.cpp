#include "Mesh.h"
#include "../loader/ModelLoader.h"
#include "../common/Path.h"

#include "../renderer/OpaqueRenderer.h"
#include "../renderer/ShadowRenderer.h"

namespace Atlas {

	namespace Mesh {

		Mesh::Mesh(const Mesh& that) {

			DeepCopy(that);

		}

		Mesh::Mesh(MeshData data, int32_t mobility) : mobility(mobility), data(data) {

			auto filename = Common::Path::GetFileName(data.filename);
			auto fileTypePos = filename.find_first_of('.');
			name = filename.substr(0, fileTypePos);

			InitializeVertexArray();

			for (auto& material : data.materials)
				AddMaterial(&material);

		}

		Mesh::Mesh(std::string filename, bool forceTangents, int32_t mobility) : mobility(mobility) {

			Loader::ModelLoader::LoadMesh(filename, data, forceTangents);

			filename = Common::Path::GetFileName(data.filename);
			auto fileTypePos = filename.find_first_of('.');
			name = filename.substr(0, fileTypePos);

			InitializeVertexArray();

			for (auto& material : data.materials)
				AddMaterial(&material);

		}

		Mesh::~Mesh() {

			ClearMaterials(configs);

		}

		Mesh& Mesh::operator=(const Mesh& that) {

			if (this != &that) {

				ClearMaterials(configs);

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

		void Mesh::UpdateMaterials() {

			auto matConfigs = configs;

			configs.clear();

			for (auto& material : data.materials)
				AddMaterial(&material);

			// Remove other configs later in case
			// some materials have unique shader permutations
			// (Because this won't recompile the shaders if materials
			// are still the same)
			ClearMaterials(matConfigs);

		}

		void Mesh::Bind() const {

			vertexArray.Bind();

		}

		void Mesh::Unbind() const {

			vertexArray.Unbind();

		}

		void Mesh::SetTransform(mat4 matrix) {

			data.SetTransform(matrix);

			UpdateData();

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
					data.indices.GetElementSize(), data.GetIndexCount(), 
					data.indices.GetConvertedVoid());
				vertexArray.AddIndexComponent(indices);
			}
			if (data.vertices.ContainsData()) {
				auto vertices = new Buffer::VertexBuffer(data.vertices.GetType(), 
					data.vertices.GetStride(), data.vertices.GetElementSize(), 
					data.GetVertexCount(), data.vertices.GetConvertedVoid());
				vertexArray.AddComponent(0, vertices);
			}
			if (data.normals.ContainsData()) {
				auto normals = new Buffer::VertexBuffer(data.normals.GetType(),
					data.normals.GetStride(), data.normals.GetElementSize(), 
					data.GetVertexCount(), data.normals.GetConvertedVoid());
				vertexArray.AddComponent(1, normals);
			}
			if (data.texCoords.ContainsData()) {
				auto texCoords = new Buffer::VertexBuffer(data.texCoords.GetType(),
					data.texCoords.GetStride(), data.texCoords.GetElementSize(),
					data.GetVertexCount(), data.texCoords.GetConvertedVoid());
				vertexArray.AddComponent(2, texCoords);
			}
			if (data.tangents.ContainsData()) {
				auto tangents = new Buffer::VertexBuffer(data.tangents.GetType(),
					data.tangents.GetStride(), data.tangents.GetElementSize(), 
					data.GetVertexCount(), data.tangents.GetConvertedVoid());
				vertexArray.AddComponent(3, tangents);
			}

			vertexArray.Unbind();

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

			if (material->HasSpecularMap()) {
				materialConfig->opaqueConfig.AddMacro("SPECULAR_MAP");
			}

			if (material->HasDisplacementMap()) {
				materialConfig->opaqueConfig.AddMacro("HEIGHT_MAP");
			}

			if (glm::length(material->emissiveColor) > 0.0f) {
				materialConfig->opaqueConfig.AddMacro("EMISSIVE");
			}

			configs[material] = materialConfig;

			Renderer::OpaqueRenderer::AddConfig(&materialConfig->opaqueConfig);
			Renderer::ShadowRenderer::AddConfig(&materialConfig->shadowConfig);

		}

		void Mesh::ClearMaterials(std::unordered_map<Material*, MaterialConfig*>& configs) {

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