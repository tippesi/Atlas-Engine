#ifndef AE_MESHDATA_H
#define AE_MESHDATA_H

#include "../System.h"
#include "../volume/AABB.h"
#include "DataComponent.h"
#include "Material.h"

#include <vector>

#define AE_PRIMITIVE_TRIANGLES GL_TRIANGLES
#define AE_PRIMITIVE_TRIANGLE_STRIP GL_TRIANGLE_STRIP

namespace Atlas {

	namespace Mesh {

		struct MeshSubData {

			uint32_t indicesOffset;
			uint32_t indicesCount;
			
			Material* material;
		
		};

		class MeshData {

		public:
			/**
			 *
			 */
			MeshData();

			MeshData(const MeshData& data);

			MeshData& operator=(const MeshData& that);

			/**
			 *
			 * @param count
			 */
			void SetIndexCount(int32_t count);

			/**
			 *
			 */
			int32_t GetIndexCount() const;

			/**
			 *
			 * @param count
			 */
			void SetVertexCount(int32_t count);

			/**
			 *
			 * @return
			 */
			int32_t GetVertexCount() const;

			DataComponent<uint32_t, void> indices;

			DataComponent<float, float> vertices;
			DataComponent<float, float16> texCoords;
			DataComponent<float, uint32_t> normals;
			DataComponent<float, uint32_t> tangents;

			std::vector<Material> materials;
			std::vector<MeshSubData> subData;

			int32_t primitiveType = AE_PRIMITIVE_TRIANGLES;

			Volume::AABB aabb;

		private:
			void DeepCopy(const MeshData& that);

			int32_t indexCount = 0;
			int32_t vertexCount = 0;

		};

	}

}

#endif