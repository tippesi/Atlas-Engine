#pragma once

#include "../System.h"
#include "TerrainStorage.h"
#include "scene/components/CameraComponent.h"

#include <vector>

namespace Atlas {

    namespace Terrain {

        class TerrainNode {

        public:
            TerrainNode(vec2 location, float height, float sideLength, int32_t LoD, int32_t LoDCount,
                int32_t LoDMultiplier, ivec2 parentIndex, ivec2 relativeIndex,
                TerrainStorage* storage, TerrainStorageCell* cell);

            ~TerrainNode();

            void Update(const CameraComponent& camera, std::vector<float>& LoDDistances,
                std::vector<TerrainNode*>& leafList, Common::Image<uint8_t>& LoDImage);

            void CheckNeighbourLoD(Common::Image<uint8_t>& LoDImage);

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