#ifndef AE_MESH_H
#define AE_MESH_H

#include "../System.h"
#include "../buffer/VertexArray.h"
#include "../shader/ShaderConfig.h"
#include "MeshData.h"
#include "Material.h"

#include <unordered_map>

#define AE_STATIONARY_MESH 0
#define AE_MOVABLE_MESH 1

namespace Atlas {

	namespace Mesh {

		class Mesh {

		public:
			/**
			 *
			 * @param data
			 * @param mobility
			 */
			Mesh(MeshData* data, int32_t mobility = AE_STATIONARY_MESH);

			/**
			 *
			 * @param filename
			 * @param mobility
			 */
			Mesh(std::string filename, int32_t mobility = AE_STATIONARY_MESH);


			~Mesh();

			/**
			 *
			 */
			void UpdateData();

			/**
			 *
			 */
			void Bind();

			/**
			 *
			 */
			void Unbind();

			MeshData* const data;

			const int32_t mobility;

			bool cullBackFaces = true;

			struct MaterialConfig {

				Shader::ShaderConfig opaqueConfig;
				Shader::ShaderConfig shadowConfig;

			};

			std::unordered_map<Material*, MaterialConfig> configs;

		private:
			void InitializeVertexArray();

			void AddMaterial(Material* material);

			Buffer::VertexArray vertexArray;

		};


	}

}

#endif