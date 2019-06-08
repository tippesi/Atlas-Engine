#ifndef AE_TERRAINNODE_H
#define AE_TERRAINNODE_H

#include "../System.h"
#include "TerrainStorage.h"
#include "Camera.h"

#include <vector>

namespace Atlas {

	namespace Terrain {

		class TerrainNode {

		public:
			TerrainNode(vec2 location, float height, float sideLength, int32_t LoD, int32_t LoDCount,
				int32_t LoDMultiplier, ivec2 parentIndex, ivec2 relativeIndex,
				TerrainStorage* storage, TerrainStorageCell* cell);

			~TerrainNode();

			void Update(Camera* camera, std::vector<float>& LoDDistances, 
				std::vector<TerrainNode*>& leafList, Common::Image8& LoDImage);

			void CheckNeighbourLoD(Common::Image8& LoDImage);

			vec2 location;
			float sideLength;
			ivec2 globalIndex;

			float leftLoDStitch = 1.0f;
			float topLoDStitch = 1.0f;
			float rightLoDStitch = 1.0f;
			float bottomLoDStitch = 1.0f;

			TerrainStorageCell* cell;

		private:
			void CreateChildren();

			ivec2 index;

			int32_t LoD;
			int32_t LoDCount;
			int32_t LoDMultiplier;
			int32_t LoDImageOffset;

			float height;

			std::vector<TerrainNode> children;

			TerrainStorage* storage;

		};

	}

}

#endif