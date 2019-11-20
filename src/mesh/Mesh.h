#ifndef AE_MESH_H
#define AE_MESH_H

#include "../System.h"
#include "../Material.h"
#include "../buffer/VertexArray.h"
#include "../shader/ShaderConfig.h"
#include "MeshData.h"

#include <unordered_map>

#define AE_STATIONARY_MESH 0
#define AE_MOVABLE_MESH 1

#define AE_OPAQUE_CONFIG 0
#define AE_SHADOW_CONFIG 1

namespace Atlas {

	namespace Mesh {

		class Mesh {

		public:
			Mesh() {}

			Mesh(const Mesh& that);

			/**
			 *
			 * @param filename
			 * @param mobility
			 */
			Mesh(MeshData data, int32_t mobility = AE_STATIONARY_MESH);

			/**
			 *
			 * @param filename
			 * @param mobility
			 */
			Mesh(std::string filename, int32_t mobility = AE_STATIONARY_MESH);

			~Mesh();

			Mesh& operator=(const Mesh& that);

			/**
			 *
			 */
			void UpdateData();

			/**
			 *
			 */
			void Bind() const;

			/**
			 *
			 */
			void Unbind() const;

			Shader::ShaderConfig* GetConfig(Material* material, int32_t type);

			MeshData data;

			int32_t mobility;

			bool cullBackFaces = true;

			struct MaterialConfig {

				Shader::ShaderConfig opaqueConfig;
				Shader::ShaderConfig shadowConfig;

			};

			std::unordered_map<Material*, MaterialConfig*> configs;

			Buffer::VertexArray vertexArray;

		private:
			void InitializeVertexArray();

			void AddMaterial(Material* material);

			void ClearMaterials();

			void DeepCopy(const Mesh& that);

		};


	}

}

#endif