#ifndef AE_GEOMETRYHELPER_H
#define AE_GEOMETRYHELPER_H

#include "../../System.h"
#include "buffer/VertexArray.h"

#include <vector>

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			class GeometryHelper {

			public:
				static void GenerateRectangleVertexArray(OldBuffer::VertexArray& vertexArray);

				static void GenerateCubeVertexArray(OldBuffer::VertexArray& vertexArray);

				static void GenerateGridVertexArray(OldBuffer::VertexArray& vertexArray, int32_t subdivisions, float scale, bool strip = true);

				static void GenerateSphereVertexArray(OldBuffer::VertexArray& vertexArray, uint32_t rings, uint32_t segments);

			private:
				static void GenerateSphere(uint32_t rings, uint32_t segments, std::vector<uint32_t>& indices,
					std::vector<vec3>& vertices, uint32_t* indexCount, uint32_t* vertexCount);

				static int8_t rectangleVertices[];

				static float cubeVertices[];

			};

		}

	}

}

#endif