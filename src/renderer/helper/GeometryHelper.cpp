#include "GeometryHelper.h"
#include "../TypeFormat.h"

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			void GeometryHelper::GenerateRectangleVertexArray(Buffer::VertexArray& vertexArray) {

				vertexArray.Bind();

				auto buffer = new Buffer::VertexBuffer(AE_BYTE, 2, sizeof(int8_t) * 2, 4);
				buffer->SetData(&rectangleVertices[0], 0, 4);
				vertexArray.AddComponent(0, buffer);

				vertexArray.Unbind();

			}

			void GeometryHelper::GenerateCubeVertexArray(Buffer::VertexArray& vertexArray) {

				vertexArray.Bind();

				auto vertexBuffer = new Buffer::VertexBuffer(AE_FLOAT, 3, sizeof(vec3), 36);
				vertexBuffer->SetData(&cubeVertices[0], 0, 36);
				vertexArray.AddComponent(0, vertexBuffer);

				vertexArray.Unbind();

			}

			void GeometryHelper::GenerateGridVertexArray(Buffer::VertexArray& vertexArray, int32_t subdivisions, float scale) {

				vertexArray.Bind();

				int32_t vertexCount = subdivisions * subdivisions;
				int32_t indexCount = (subdivisions * 2 + 2) * (subdivisions - 1);

				auto vertexBuffer = new Buffer::VertexBuffer(AE_FLOAT, 3, sizeof(vec3), vertexCount);
				auto indexBuffer = new Buffer::IndexBuffer(AE_UINT, sizeof(uint32_t), indexCount);

				std::vector<vec3> vertices(vertexCount);
				std::vector<uint32_t> indices(indexCount);

				for (int32_t z = 0; z < subdivisions; z++) {
					for (int32_t x = 0; x < subdivisions; x++) {

						auto xf = (float)z * scale;
						auto zf = (float)x * scale;

						vertices[z * subdivisions + x] = vec3(xf, 0.0f, zf);

					}
				}

				vertexBuffer->SetData(vertices.data(), 0, vertexCount);

				int32_t i = 0;

				for (int32_t z = 0; z < subdivisions - 1; z++) {

					indices[i++] = z * subdivisions;

					for (int32_t x = 0; x < subdivisions; x++) {
						indices[i++] = z * subdivisions + x;
						indices[i++] = (z + 1) * subdivisions + x;
					}

					indices[i++] = (z + 1) * subdivisions + (subdivisions - 1);

				}

				indexBuffer->SetData(indices.data(), 0, indexCount);

				vertexArray.AddIndexComponent(indexBuffer);
				vertexArray.AddComponent(0, vertexBuffer);

				vertexArray.Unbind();

			}

			void GeometryHelper::GenerateSphereVertexArray(Buffer::VertexArray& vertexArray, uint32_t rings, uint32_t segments) {

				std::vector<uint32_t> indices;
				std::vector<vec3> vertices;

				uint32_t indexCount, vertexCount;

				// The sphere is generated with triangles that are in clockwise order
				// This helps us for both atmospheric and point light rendering
				GenerateSphere(rings, segments, indices, vertices, &indexCount, &vertexCount);

				auto indicesBuffer = new Buffer::IndexBuffer(AE_UINT, sizeof(uint32_t), indexCount);
				auto verticesBuffer = new Buffer::VertexBuffer(AE_FLOAT, 3, sizeof(vec3), vertexCount);
				indicesBuffer->SetData(indices.data(), 0, indexCount);
				verticesBuffer->SetData(vertices.data(), 0, vertexCount);
				vertexArray.AddIndexComponent(indicesBuffer);
				vertexArray.AddComponent(0, verticesBuffer);

			}

			void GeometryHelper::GenerateSphere(uint32_t rings, uint32_t segments, std::vector<uint32_t>& indices,
				std::vector<vec3>& vertices, uint32_t* indexCount, uint32_t* vertexCount) {

				rings = rings < 3 ? 3 : rings;
				segments = segments < 3 ? 3 : segments;

				uint32_t innerRings = rings - 2;
				*vertexCount = innerRings * segments + 2;
				*indexCount = (innerRings - 1) * segments * 6 + 2 * segments * 3;

				vertices.resize(*vertexCount);
				indices.resize(*indexCount);

				// Set the two outer rings
				vertices[0] = vec3(0.0f, 1.0f, 0.0f);
				vertices[*vertexCount - 1] = vec3(0.0f, -1.0f, 0.0f);

				const float pi = 3.14159265359f;
				float fRings = (float)rings;
				float fSegments = (float)segments;

				uint32_t vertexIndex = 1;

				float alpha = pi / (fRings - 1);

				// Generate the vertices for the inner rings
				for (uint32_t i = 0; i < innerRings; i++) {

					float ringRadius = sinf(alpha);
					float ringHeight = cosf(alpha);
					float beta = 0.0f;

					for (uint32_t j = 0; j < segments; j++) {

						float x = sinf(beta) * ringRadius;
						float z = cosf(beta) * ringRadius;
						vertices[vertexIndex++] = vec3(x, ringHeight, z);

						beta += 2.0f * pi / fSegments;

					}

					alpha += pi / (fRings - 1);

				}

				uint32_t indexIndex = 0;

				// Indices for the upper outer ring
				for (uint32_t i = 0; i < segments; i++) {
					indices[indexIndex++] = i + 1;
					indices[indexIndex++] = 0;
					indices[indexIndex++] = (i + 1) % segments + 1;
				}

				// Indices for the lower outer ring
				for (uint32_t i = 0; i < segments; i++) {
					indices[indexIndex++] = *vertexCount - 1 - segments + (i + 1) % segments;
					indices[indexIndex++] = *vertexCount - 1;
					indices[indexIndex++] = *vertexCount - 1 - segments + i;
				}

				uint32_t offset = 1;

				// Generate the indices for the inner rings
				for (uint32_t i = 0; i < innerRings - 1; i++) {
					offset += segments;
					for (uint32_t j = 0; j < segments; j++) {
						indices[indexIndex++] = offset - segments + j;
						indices[indexIndex++] = offset + (j + 1) % segments;
						indices[indexIndex++] = offset + j;
						indices[indexIndex++] = offset - segments + (j + 1) % segments;
						indices[indexIndex++] = offset + (j + 1) % segments;
						indices[indexIndex++] = offset - segments + j;
					}
				}

			}

			int8_t GeometryHelper::rectangleVertices[] = {
					-1, -1,
					1, -1,
					-1, 1,
					1, 1
			};

			float GeometryHelper::cubeVertices[] = {
					-1.0f,  1.0f, -1.0f,
					-1.0f, -1.0f, -1.0f,
					1.0f, -1.0f, -1.0f,
					1.0f, -1.0f, -1.0f,
					1.0f,  1.0f, -1.0f,
					-1.0f,  1.0f, -1.0f,

					-1.0f, -1.0f,  1.0f,
					-1.0f, -1.0f, -1.0f,
					-1.0f,  1.0f, -1.0f,
					-1.0f,  1.0f, -1.0f,
					-1.0f,  1.0f,  1.0f,
					-1.0f, -1.0f,  1.0f,

					1.0f, -1.0f, -1.0f,
					1.0f, -1.0f,  1.0f,
					1.0f,  1.0f,  1.0f,
					1.0f,  1.0f,  1.0f,
					1.0f,  1.0f, -1.0f,
					1.0f, -1.0f, -1.0f,

					-1.0f, -1.0f,  1.0f,
					-1.0f,  1.0f,  1.0f,
					1.0f,  1.0f,  1.0f,
					1.0f,  1.0f,  1.0f,
					1.0f, -1.0f,  1.0f,
					-1.0f, -1.0f,  1.0f,

					-1.0f,  1.0f, -1.0f,
					1.0f,  1.0f, -1.0f,
					1.0f,  1.0f,  1.0f,
					1.0f,  1.0f,  1.0f,
					-1.0f,  1.0f,  1.0f,
					-1.0f,  1.0f, -1.0f,

					-1.0f, -1.0f, -1.0f,
					-1.0f, -1.0f,  1.0f,
					1.0f, -1.0f, -1.0f,
					1.0f, -1.0f, -1.0f,
					-1.0f, -1.0f,  1.0f,
					1.0f, -1.0f,  1.0f
			};

		}

	}

}