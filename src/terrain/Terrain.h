#ifndef AE_TERRAIN_H
#define AE_TERRAIN_H

#include "../System.h"
#include "TerrainNode.h"
#include "TerrainStorage.h"

#include "../Camera.h"
#include "buffer/VertexArray.h"

#include <vector>

namespace Atlas {

	namespace Terrain {

		/**
		 * Represents a terrain. This class uses different LoDs (quad tree) and tessellation to render the terrain
		 * efficiently with high detail without sacrificing too much on performance.
		 */
		class Terrain {

		public:

			/**
             * Constructs a Terrain object.
             * @param rootNodeSideCount The number of root nodes per side. Root nodes are the lowest level of detail.
             * @param LoDCount The number of level of detail levels. Results in LoDCount - 1 subdivisions of the root nodes.
             * @param patchSizeFactor Changes the size of a patch and the number of vertices in a patch
             * @param resolution Like a scale in x and z direction of the terrain. Lower means more vertices per square unit.
             * @param height The maximum height of the terrain
             * @note The maximum number of nodes in a terrain is 2^16. A node has 8 * 8 = 64 patches.
             * The number of vertices per patch corresponds to 4 * pow(2 * patchSizeFactor, 2).
             * @remark Let's assume we have a rootNodeSideCount of 3 which leads to rootNodeCount = 3^2 = 9.
             * We now decide that we want an LoDCount of 7. To check if we don't exceed the maximum number of nodes we calculate
             * rootNodeCount * (pow(4,LoDCount) - 1) / 3 = 3 * (pow(4, 7) - 1) = 49149 < 2^16 = 65536. This means that at the
             * maximum lod which is Lod6 we have a total of pow(4, 6) * 9 = 36864 nodes. If we have a patchSizeFactor of 4
             * we have 4 * pow(2 * 4, 2) = 64 vertices per patch. This results in a maximum vertex count of
             * around 600 mio. If we take the square root we get the number of vertices per side: sqrt(603979776) = 24576. If we set
             * the resolution to 0.5 we get a total of 12288 units per terrain side. The size of map is therefore roughly
             * 150 kilounits^2.
             */
			Terrain(int32_t rootNodeSideCount, int32_t LoDCount, int32_t patchSizeFactor, float resolution, float height);

			/**
             * Updates the terrain and the storage queues.
             * @param camera
             * @note After calling this method the storage->requestedCells list contains all the
             * StorageCells which are needed by the terrain. If these StorageCells aren't loaded the terrain
             * can't increase the level of detail of the specific nodes. The storage->unusedCells list contains
             * all the cells which aren't needed any more at the moment.
             */
			void Update(Camera* camera);

			/**
             * Sets the distance of a specific level of detail.
             * @param LoD The level of detail to be set in range of (0,LoDCount-1)
             * @param distance The distance where the level of details should begin
             * @note Only the highest level of detail will render splatmaps and tessellation.
             */
			void SetLoDDistance(int32_t LoD, float distance);

			float GetLoDDistance(int32_t LoD);

			/**
             * Sets the tessellation function t(distance) = factor / pow(distance, slope) + shift
             * @param factor
             * @param slope
             * @param shift
             * @param maxLevel Determines the maximum level of subdivisions the tessellation uses.
             * @remarks The tessellation for a point p is calculated by:
             * {@code
             * distance = length(camera->location, p);
             * tessLevel = mix(1.0f, maxLevel, clamp(t(distance), 0.0f, 1.0f));
             * }
             */
			void SetTessellationFunction(float factor, float slope, float shift, float maxLevel = 64.0f);

			/**
             * Sets the distance where the displacement should start
             * @param distance The distance
             * @note Displacement mapping is only available if tessellation is enabled in the specified distance.
             * The maximum distance recommended is where t(distance) = 1, where t is the tessellation function.
             * Otherwise there would be too much flickering present on screen.
             */
			void SetDisplacementDistance(float distance);

			/**
            * Gets the height at a specific point on the terrain.
            * @param x The x component of the point relative to the terrain origin
            * @param z The z component of the point relative to the terrain origin
            * @return The height of the terrain.
            * @note This function is computationally expensive.
            * @warning The cell containing the height value has to be loaded.
            */
			float GetHeight(float x, float z);

			/**
            * Gets the storage cell for a point on the terrain.
            * @param x The x component of the point relative to the terrain origin
            * @param z The z component of the point relative to the terrain origin
            * @param LoD The lod level of the storage cell.
            * @return A pointer to a TerrainStorageCell object. Equals nullptr if input wasn't valid.
            * @note For every point there exist as many storage cells as there are lod levels.
            */
			TerrainStorageCell* GetStorageCell(float x, float z, int32_t LoD);

			/**
             * Binds the vertex array of the terrain
             */
			void Bind();

			/**
             * Unbinds the vertex array of the terrain
             */
			void Unbind();

			TerrainStorage* storage;

			const int32_t rootNodeSideCount;
			const int32_t LoDCount;
			const int32_t patchSizeFactor;

			vec3 translation;
			float resolution;
			float sideLength;

			int32_t patchVertexCount;

			float tessellationFactor;
			float tessellationSlope;
			float tessellationShift;
			float maxTessellationLevel;

			float heightScale;
			float displacementDistance;

			std::vector<TerrainNode*> renderList;

			Common::Image8 LoDImage;

		private:
			void SortNodes(std::vector<TerrainNode*>& nodes, vec3 cameraLocation);

			void GeneratePatchVertexBuffer();

			void GeneratePatchOffsets();

			float BarryCentric(vec3 p1, vec3 p2, vec3 p3, vec2 pos);

			int32_t rootNodeCount;

			std::vector<vec2> vertices;
			std::vector<vec2> patchOffsets;

			Buffer::VertexArray vertexArray;

			std::vector<float> LoDDistances;
			std::vector<TerrainNode> rootNodes;

		};

	}

}

#endif