#include "Terrain.h"

namespace Atlas {

	namespace Terrain {

		Terrain::Terrain(int32_t rootNodeSideCount, int32_t LoDCount, int32_t patchSizeFactor, float resolution,
			float heightScale) : rootNodeSideCount(rootNodeSideCount), LoDCount(LoDCount), 
			patchSizeFactor(patchSizeFactor), resolution(resolution), heightScale(heightScale) {

			translation = vec3(0.0f);

			rootNodeCount = rootNodeSideCount * rootNodeSideCount;

			// sum{k = 0 to LODCount - 1} 4^k = (4^(LODCount) - 1) / 3
			int32_t nodesCount = (int32_t) ((powf(4.0f, (float) LoDCount) - 1.0f) / 3.0f) * this->rootNodeCount;

			// We can just have 2^16 nodes due to 16 bit indexing
			if (nodesCount >= 65536) {
				throw AtlasException("Wasn't able to create terrain due to too many nodes");
			}

			GeneratePatchVertexBuffer();
			GeneratePatchOffsets();

			storage = new TerrainStorage(this->rootNodeCount, this->LoDCount);
			LoDDistances = std::vector<float>(LoDCount);

			// 2.0f time because we generate 2 * patchSize vertices. 8.0f because we have 8 * 8 patches per node.
			sideLength = (float)rootNodeSideCount * resolution * powf(2, (float) this->LoDCount - 1.0f) * 2.0f *
						 patchSizeFactor * 8.0f;
			float ratio = sideLength / (float)rootNodeSideCount;

			for (int32_t i = 0; i < this->LoDCount; i++) {
				LoDDistances[i] = sideLength - powf((float) i / (float) this->LoDCount, 0.25f) * sideLength;
			}

			for (int32_t i = 0; i < rootNodeSideCount; i++) {
				for (int32_t j = 0; j < rootNodeSideCount; j++) {
					TerrainStorageCell *cell = storage->GetCell(i, j, 0);
					storage->requestedCells.push_back(cell);
					rootNodes.push_back(
							new TerrainNode(vec2((float) i * ratio, (float) j * ratio), resolution, heightScale, ratio,
											0, this->LoDCount, ivec2(0, 0), ivec2(i, j), storage, cell));
				}
			}

			tessellationFactor = 0.0f;
			tessellationSlope = 1.0f;
			tessellationShift = 0.0f;
			maxTessellationLevel = 1;

			displacementDistance = 0.0f;

		}

		void Terrain::Update(Camera *camera) {

			renderList.clear();

			for (TerrainNode *&node : rootNodes) {
				node->Update(camera, renderList, LoDDistances.data());
			}

			// TODO: Sort renderlist by LoD here. Better: Have a list for each LoD

		}

		void Terrain::SetLoDDistance(int32_t LoD, float distance) {

			if (LoD >= 0 && LoD < LoDCount) {
				LoDDistances[LoD] = distance;
			}

		}

		float Terrain::GetLoDDistance(int32_t LoD) {

			if (LoD >= 0 && LoD < LoDCount) {
				return LoDDistances[LoD];
			}

			return 0.0f;

		}

		void Terrain::SetTessellationFunction(float factor, float slope, float shift, float maxLevel) {

			tessellationFactor = factor;
			tessellationSlope = slope;
			tessellationShift = shift;

			maxTessellationLevel = glm::clamp(maxLevel, 1.0f, 64.0f);

		}

		void Terrain::SetDisplacementDistance(float distance) {

			displacementDistance = distance;

		}

		float Terrain::GetHeight(float x, float z) {

			if (x < 0.0f || z < 0.0f || x > sideLength || z > sideLength) {
				return 0.0f;
			}

			float nodeSideLength = 16.0f * patchSizeFactor * resolution;

			x /= nodeSideLength;
			z /= nodeSideLength;

			float xIndex = floorf(x);
			float zIndex = floorf(z);

			auto cell = storage->GetCell((int32_t) xIndex, (int32_t) zIndex, LoDCount - 1);

			if (cell == nullptr) {
				return 0.0f;
			}

			if (cell->heightData.size() == 0 || cell->heightField == nullptr) {
				return 0.0f;
			}

			if ((int32_t) cell->heightData.size() != cell->heightField->height * cell->heightField->width) {
				return 0.0f;
			}

			x *= nodeSideLength;
			z *= nodeSideLength;

			float xOffset = fmod(x, nodeSideLength);
			float zOffset = fmod(z, nodeSideLength);

			float xGridSize = nodeSideLength / (float) cell->heightField->width;
			float zGridSize = nodeSideLength / (float) cell->heightField->height;

			float xCoord = fmod(xOffset, xGridSize) / xGridSize;
			float zCoord = fmod(zOffset, zGridSize) / zGridSize;

			float height = 0.0f;

			int32_t xPosition = (int32_t) (xOffset / xGridSize);
			int32_t zPosition = (int32_t) (zOffset / zGridSize);

			/*
            topLeft        topRight
            c##############c
            ################
            ####p###########
            c##############c
            bottomLeft     bottomRight
            p is the point with location (x, z)
            topLeft is in x direction
            bottomRight is in z direction
            */
			float heightBottomLeft = (float) cell->heightData[xPosition + cell->heightField->width * zPosition];

			float heightBottomRight = 0.0f;
			float heightTopRight = 0.0f;
			float heightTopLeft = 0.0f;

			// Check if we must sample from a neighbour node (allows for errors while retrieving the height information at the edge of the terrain)
			if (zPosition + 1 == cell->heightField->height &&
				xPosition + 1 == cell->heightField->width) {
				auto neighbourCell = storage->GetCell((int32_t) xIndex + 1, (int32_t) zIndex + 1, LoDCount - 1);

				if (neighbourCell == nullptr) {
					heightTopLeft = heightBottomLeft;
					heightTopRight = heightBottomLeft;
					heightBottomRight = heightBottomLeft;
				} else {
					if (neighbourCell->heightData.size() == 0 || neighbourCell->heightField == nullptr) {
						return 0.0f;
					}

					heightTopLeft = (float) neighbourCell->heightData[1];
					heightBottomRight = (float) neighbourCell->heightData[neighbourCell->heightField->width];
					heightTopRight = (float) neighbourCell->heightData[neighbourCell->heightField->width + 1];
				}
			} else if (zPosition + 1 == cell->heightField->height) {

				heightTopLeft = (float) cell->heightData[xPosition + 1 + cell->heightField->width * zPosition];

				auto neighbourCell = storage->GetCell((int32_t) xIndex, (int32_t) zIndex + 1, LoDCount - 1);

				if (neighbourCell == nullptr) {
					heightBottomRight = heightBottomLeft;
					heightTopRight = heightTopLeft;
				} else {
					if (neighbourCell->heightData.size() == 0 || neighbourCell->heightField == nullptr) {
						return 0.0f;
					}

					heightBottomRight = (float) neighbourCell->heightData[xPosition];
					heightTopRight = (float) neighbourCell->heightData[xPosition + 1];
				}

			} else if (xPosition + 1 == cell->heightField->width) {

				heightBottomRight = (float) cell->heightData[xPosition + cell->heightField->width * (zPosition + 1)];

				auto neighbourCell = storage->GetCell((int32_t) xIndex + 1, (int32_t) zIndex, LoDCount - 1);

				if (neighbourCell == nullptr) {
					heightTopLeft = heightBottomLeft;
					heightTopRight = heightBottomRight;
				} else {
					if (neighbourCell->heightData.size() == 0 || neighbourCell->heightField == nullptr) {
						return 0.0f;
					}

					heightTopLeft = (float) neighbourCell->heightData[zPosition * neighbourCell->heightField->width];
					heightTopRight = (float) neighbourCell->heightData[(zPosition + 1) *
																	   neighbourCell->heightField->width];
				}

			} else {
				heightTopLeft = (float) cell->heightData[xPosition + 1 + cell->heightField->width * zPosition];
				heightBottomRight = (float) cell->heightData[xPosition + cell->heightField->width * (zPosition + 1)];
				heightTopRight = (float) cell->heightData[xPosition + 1 + cell->heightField->width * (zPosition + 1)];
			}

			if (xCoord > zCoord) {
				height = BarryCentric(vec3(0.0f, heightBottomLeft / 65535.0f * heightScale, 0.0f),
									  vec3(1.0f, heightTopLeft / 65535.0f * heightScale, 0.0f),
									  vec3(1.0f, heightTopRight / 65535.0f * heightScale, 1.0f),
									  vec2(xCoord, zCoord));
			} else {
				height = BarryCentric(vec3(0.0f, heightBottomLeft / 65535.0f * heightScale, 0.0f),
									  vec3(1.0f, heightTopRight / 65535.0f * heightScale, 1.0f),
									  vec3(0.0f, heightBottomRight / 65535.0f * heightScale, 1.0f),
									  vec2(xCoord, zCoord));
			}

			return height;

		}

		TerrainStorageCell *Terrain::GetStorageCell(float x, float z, int32_t LoD) {

			float nodeSideLength = 16.0f * patchSizeFactor * resolution;

			x /= nodeSideLength;
			z /= nodeSideLength;

			float xIndex = floorf(x);
			float zIndex = floorf(z);

			return storage->GetCell((int32_t) xIndex, (int32_t) zIndex, LoD);

		}

		void Terrain::Bind() {

			vertexArray.Bind();

		}

		void Terrain::Unbind() {

			vertexArray.Unbind();

		}

		void Terrain::GeneratePatchVertexBuffer() {

			patchVertexCount = 4 * (int32_t) powf((float) (patchSizeFactor), 2.0f);

			vertices = std::vector<vec2>(patchVertexCount);

			int32_t index = 0;

			for (int32_t x = 0; x < patchSizeFactor; x++) {
				for (int32_t y = 0; y < patchSizeFactor; y++) {
					vertices[index++] = vec2(0.0f, 0.0f) + vec2((float) x, (float) y);
					vertices[index++] = vec2(1.0f, 0.0f) + vec2((float) x, (float) y);
					vertices[index++] = vec2(1.0f, 1.0f) + vec2((float) x, (float) y);
					vertices[index++] = vec2(0.0f, 1.0f) + vec2((float) x, (float) y);
				}
			}

			auto vertexBuffer = new Buffer::VertexBuffer(AE_FLOAT, 2, sizeof(vec2), patchVertexCount);
			vertexBuffer->SetData(&vertices.data()[0], 0, patchVertexCount);
			vertexArray.AddComponent(0, vertexBuffer);
			glPatchParameteri(GL_PATCH_VERTICES, 4);

		}

		void Terrain::GeneratePatchOffsets() {

			patchOffsets = std::vector<vec2>(64);

			int32_t index = 0;

			for (int32_t x = 0; x < 8; x++) {
				for (int32_t y = 0; y < 8; y++) {
					patchOffsets[index++] = vec2((float) x, (float) y);
				}
			}

			auto offsetBuffer = new Buffer::VertexBuffer(AE_FLOAT, 2, sizeof(vec2), 64);
			offsetBuffer->SetData(&patchOffsets.data()[0], 0, 64);
			vertexArray.AddInstancedComponent(1, offsetBuffer);

		}

		float Terrain::BarryCentric(vec3 p1, vec3 p2, vec3 p3, vec2 pos) {

			float det = (p2.z - p3.z) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.z - p3.z);
			float l1 = ((p2.z - p3.z) * (pos.x - p3.x) + (p3.x - p2.x) * (pos.y - p3.z)) / det;
			float l2 = ((p3.z - p1.z) * (pos.x - p3.x) + (p1.x - p3.x) * (pos.y - p3.z)) / det;
			float l3 = 1.0f - l1 - l2;
			return l1 * p1.y + l2 * p2.y + l3 * p3.y;


		}

	}

}