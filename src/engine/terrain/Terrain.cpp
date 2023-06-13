#include "Terrain.h"

#include "../renderer/helper/GeometryHelper.h"

#include <algorithm>

namespace Atlas {

    namespace Terrain {

        Terrain::Terrain(int32_t rootNodeSideCount, int32_t LoDCount, int32_t patchSizeFactor, float resolution,
            float heightScale) : rootNodeSideCount(rootNodeSideCount), LoDCount(LoDCount), 
            patchSizeFactor(patchSizeFactor), resolution(resolution), heightScale(heightScale) {

            translation = vec3(0.0f);

            rootNodeCount = rootNodeSideCount * rootNodeSideCount;

            // sum{k = 0 to LODCount - 1} 4^k = (4^(LODCount) - 1) / 3
            int32_t nodesCount = (int32_t) ((powf(4.0f, (float) LoDCount) - 1.0f) / 3.0f) * rootNodeCount;
            int32_t leafNodesSideCount = rootNodeSideCount * (int32_t)(powf(2.0f, (float)LoDCount - 1.0f));

            GeneratePatchVertexBuffer();
            GeneratePatchOffsets();

            Renderer::Helper::GeometryHelper::GenerateGridVertexArray(distanceVertexArray, 
                8 * patchSizeFactor + 1, 1.0f);

            // 2.0f time because we generate 2 * patchSize vertices. 8.0f because we have 8 * 8 patches per node.
            sideLength = (float)rootNodeSideCount * resolution * powf(2, (float) LoDCount - 1.0f) *
                         patchSizeFactor * 8.0f;
            float ratio = sideLength / (float)rootNodeSideCount;

            storage = TerrainStorage(rootNodeCount, LoDCount, sideLength, 1024, 32);
            LoDDistances = std::vector<float>(LoDCount);
            LoDImage = Common::Image<uint8_t>(leafNodesSideCount, leafNodesSideCount, 1);

            auto distance = sideLength;

            for (int32_t i = 0; i < LoDCount; i++) {
                distance /= 2.0f;
                LoDDistances[i] = distance;                
            }

            for (int32_t i = 0; i < rootNodeSideCount; i++) {
                for (int32_t j = 0; j < rootNodeSideCount; j++) {
                    TerrainStorageCell *cell = storage.GetCell(i, j, 0);
                    storage.requestedCells.push_back(cell);
                    rootNodes.push_back(TerrainNode(vec2((float) i * ratio, (float) j * ratio), heightScale,
                        ratio, 0, LoDCount, rootNodeSideCount, ivec2(0, 0), ivec2(i, j), &storage, cell));
                }
            }

            tessellationFactor = 8000.0f;
            tessellationSlope = 3.0f;
            tessellationShift = 0.0f;
            maxTessellationLevel = 16;

            displacementDistance = 20.0f;

        }

        void Terrain::Update(Camera *camera) {

            leafList.clear();

            for (auto& node : rootNodes)
                node.Update(camera, LoDDistances,
                    leafList, LoDImage);

        }

        void Terrain::UpdateRenderlist(Volume::Frustum* frustum, vec3 location) {

            renderList.clear();

            for (auto node : leafList) {
                auto aabb = Volume::AABB(
                    vec3(node->location.x, 0.0f, node->location.y),
                    vec3(node->location.x + node->sideLength, heightScale,
                        node->location.y + node->sideLength)
                );

                if (frustum->Intersects(aabb))
                    renderList.push_back(node);
            }

            for (auto node : renderList) {
                node->CheckNeighbourLoD(LoDImage);
            }

            // Sort the list to render from front to back
            SortNodes(renderList, location);

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

        float Terrain::GetHeight(float x, float y) {

            vec3 normal, forward;
            return GetHeight(x, y, normal, forward);

        }

        float Terrain::GetHeight(float x, float z, vec3& normal, vec3& forward) {

            if (x < 0.0f || z < 0.0f || x > sideLength || z > sideLength)
                return 0.0f;

            float nodeSideLength = 8.0f * patchSizeFactor * resolution;

            x /= nodeSideLength;
            z /= nodeSideLength;

            float xPosition = floorf(x);
            float zPosition = floorf(z);

            auto cell = storage.GetCell(int32_t(xPosition), int32_t(zPosition), LoDCount - 1);

            if (!cell)
                return 0.0f;

            x -= xPosition;
            z -= zPosition;

            // Cells have overlapping edges (last pixels are on next cell)
            x *= float(cell->heightField.width - 1);
            z *= float(cell->heightField.height - 1);

            xPosition = floorf(x);
            zPosition = floorf(z);

            auto xCoord = x - xPosition;
            auto zCoord = z - zPosition;

            int32_t xIndex = int32_t(xPosition);
            int32_t zIndex = int32_t(zPosition);

            float height = 0.0f;

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
            float heightBottomLeft = cell->heightData[xIndex + cell->heightField.width * zIndex];
            float heightBottomRight = 0.0f;
            float heightTopRight = 0.0f;
            float heightTopLeft = 0.0f;

            // Check if we must sample from a neighbour node (allows for errors while retrieving the height information at the edge of the terrain)
            if (zIndex + 1 == cell->heightField.height &&
                xIndex + 1 == cell->heightField.width) {
                auto neighbourCell = storage.GetCell(xIndex + 1, zIndex + 1, LoDCount - 1);

                if (!neighbourCell) {
                    heightTopLeft = heightBottomLeft;
                    heightTopRight = heightBottomLeft;
                    heightBottomRight = heightBottomLeft;
                }
                else {
                    heightTopLeft = neighbourCell->heightData[1];
                    heightBottomRight = neighbourCell->heightData[neighbourCell->heightField.width];
                    heightTopRight = neighbourCell->heightData[neighbourCell->heightField.width + 1];
                }
            }
            else if (zIndex + 1 == cell->heightField.height) {

                heightTopLeft = cell->heightData[xIndex + 1 + cell->heightField.width * zIndex];

                auto neighbourCell = storage.GetCell(xIndex, zIndex + 1, LoDCount - 1);

                if (neighbourCell == nullptr) {
                    heightBottomRight = heightBottomLeft;
                    heightTopRight = heightTopLeft;
                }
                else {
                    heightBottomRight = neighbourCell->heightData[xIndex];
                    heightTopRight = neighbourCell->heightData[xIndex + 1];
                }

            }
            else if (xIndex + 1 == cell->heightField.width) {

                heightBottomRight = cell->heightData[xIndex + cell->heightField.width * (zIndex + 1)];

                auto neighbourCell = storage.GetCell(xIndex + 1, zIndex, LoDCount - 1);

                if (neighbourCell == nullptr) {
                    heightTopLeft = heightBottomLeft;
                    heightTopRight = heightBottomRight;
                }
                else {
                    heightTopLeft = neighbourCell->heightData[zIndex * neighbourCell->heightField.width];
                    heightTopRight = neighbourCell->heightData[(zIndex + 1) *
                        neighbourCell->heightField.width];
                }

            }
            else {
                heightTopLeft = cell->heightData[xIndex + 1 + cell->heightField.width * zIndex];
                heightBottomRight = cell->heightData[xIndex + cell->heightField.width * (zIndex + 1)];
                heightTopRight = cell->heightData[xIndex + 1 + cell->heightField.width * (zIndex + 1)];
            }

            heightBottomLeft *= heightScale;
            heightBottomRight *= heightScale;
            heightTopLeft *= heightScale;
            heightTopRight *= heightScale;

            if (xCoord > zCoord) {
                height = BarryCentric(vec3(0.0f, heightBottomLeft, 0.0f),
                                      vec3(1.0f, heightTopLeft, 0.0f),
                                      vec3(1.0f, heightTopRight, 1.0f),
                                      vec2(xCoord, zCoord));
                forward = -glm::normalize(vec3(0.0f, heightBottomLeft, 0.0f) -
                    vec3(1.0f, heightTopLeft, 0.0f));
                auto right = glm::normalize(vec3(1.0f, heightTopRight, 1.0f) -
                    vec3(1.0f, heightTopLeft, 0.0f));
                normal = -glm::normalize(glm::cross(forward, right));
            } else {
                height = BarryCentric(vec3(0.0f, heightBottomLeft, 0.0f),
                                      vec3(1.0f, heightTopRight, 1.0f),
                                      vec3(0.0f, heightBottomRight, 1.0f),
                                      vec2(xCoord, zCoord));
                forward = glm::normalize(vec3(1.0f, heightTopRight, 1.0f) -
                    vec3(0.0f, heightBottomRight, 1.0f));
                auto right = -glm::normalize(vec3(0.0f, heightBottomLeft, 0.0f) -
                    vec3(0.0f, heightBottomRight, 1.0f));
                normal = -glm::normalize(glm::cross(forward, right));
            }

            return height;

        }

        vec2 Terrain::GetGradient(float x, float z) {

            vec3 normal, forward;
            GetHeight(x, z, normal, forward);

            auto right = glm::cross(forward, normal);

            auto deltaX = forward.y / forward.x;
            auto deltaZ = right.y / right.z;

            return vec2(deltaX, deltaZ);

        }

        TerrainStorageCell *Terrain::GetStorageCell(float x, float z, int32_t LoD) {

            float nodeSideLength = 8.0f * patchSizeFactor * resolution;

            x /= nodeSideLength;
            z /= nodeSideLength;

            float xIndex = floorf(x);
            float zIndex = floorf(z);

            return storage.GetCell((int32_t) xIndex, (int32_t) zIndex, LoD);

        }

        void Terrain::SortNodes(std::vector<TerrainNode*>& nodes, vec3 cameraLocation) {

            std::sort(nodes.begin(), nodes.end(),
                [=](TerrainNode* node1, TerrainNode* node2) -> bool {

                    auto distance1 = glm::distance(cameraLocation,
                        vec3(node1->location.x + node1->sideLength / 2.0f,
                            0.0f, node1->location.y + node1->sideLength / 2.0f));
                    auto distance2 = glm::distance(cameraLocation,
                        vec3(node2->location.x + node2->sideLength / 2.0f,
                            0.0f, node2->location.y + node2->sideLength / 2.0f));

                    return distance1 < distance2;

                });

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

            auto buffer = Buffer::VertexBuffer(VK_FORMAT_R32G32_SFLOAT, patchVertexCount);
            buffer.SetData(&vertices.data()[0], 0, patchVertexCount);
            vertexArray.AddComponent(0, buffer);

        }

        void Terrain::GeneratePatchOffsets() {

            patchOffsets = std::vector<vec2>(64);

            int32_t index = 0;

            for (int32_t x = 0; x < 8; x++) {
                for (int32_t y = 0; y < 8; y++) {
                    patchOffsets[index++] = vec2((float) x, (float) y);
                }
            }

            auto buffer = Buffer::VertexBuffer(VK_FORMAT_R32G32_SFLOAT, 64);
            buffer.SetData(&vertices.data()[0], 0, 64);
            vertexArray.AddInstancedComponent(1, buffer);

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