#ifndef AE_MESHDATA_H
#define AE_MESHDATA_H

#include "../System.h"
#include "../common/AABB.h"
#include "DataComponent.h"
#include "Material.h"

#include <vector>

#define AE_PRIMITIVE_TRIANGLES GL_TRIANGLES
#define AE_PRIMITIVE_TRIANGLE_STRIP GL_TRIANGLE_STRIP

namespace Atlas {

	namespace Mesh {

		struct MeshSubData {
			uint32_t indicesOffset;
			uint32_t numIndices;
			uint32_t materialIndex;
		};

		class MeshData {

		public:
			/**
			 *
			 */
			MeshData();

			~MeshData();

			/**
			 *
			 * @param count
			 */
			void SetIndexCount(int32_t count);

			/**
			 *
			 */
			int32_t GetIndexCount();

			/**
			 *
			 * @param count
			 */
			void SetVertexCount(int32_t count);

			/**
			 *
			 * @return
			 */
			int32_t GetVertexCount();

			DataComponent<uint32_t, void>* indices;

			DataComponent<float, float>* vertices;
			DataComponent<float, float16>* texCoords;
			DataComponent<float, uint32_t>* normals;
			DataComponent<float, uint32_t>* tangents;

			std::vector<Material*> materials;
			std::vector<MeshSubData*> subData;

			int32_t primitiveType;

			Common::AABB aabb;

		private:
			int32_t indexCount;
			int32_t vertexCount;

		};

	}

}

#endif