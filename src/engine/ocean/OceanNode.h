#pragma once

#include "System.h"
#include "common/Image.h"
#include "scene/components/CameraComponent.h"

#include <vector>

namespace Atlas {

    namespace Ocean {

        class OceanNode {

        public:
            OceanNode() = default;

            OceanNode(vec2 location, float sideLength, int32_t LoD, int32_t LoDCount,
                int32_t LoDMultiplier, ivec2 parentIndex, ivec2 localIndex);

            void CheckNeighbourLoD(Common::Image<uint8_t>& LoDImage);

            vec2 location = vec2(0.0f);
            float sideLength = 0.0f;
            ivec2 globalIndex = ivec2(0);

            float leftLoDStitch = 1.0f;
            float topLoDStitch = 1.0f;
            float rightLoDStitch = 1.0f;
            float bottomLoDStitch = 1.0f;

        protected:
            void Update(const CameraComponent& camera, vec3 translation, std::vector<float>& LoDDistances,
                std::vector<OceanNode*>& leafList, Common::Image<uint8_t>& LoDImage);

            std::vector<OceanNode> children;

            ivec2 localIndex = ivec2(0);

            int32_t LoD = 0;
            int32_t LoDCount = 0;
            int32_t LoDMultiplier = 0;
            int32_t LoDImageOffset = 0;

        private:
            void CreateChildren();

        };

    }

}