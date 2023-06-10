#ifndef AE_OCEAN_H
#define AE_OCEAN_H

#include "../System.h"
#include "../Camera.h"

#include "OceanState.h"
#include "OceanNode.h"
#include "OceanSimulation.h"

#include <vector>

namespace Atlas {

    namespace Ocean {

        class Ocean : public OceanNode {

        public:
            Ocean() = default;

            Ocean(int32_t LoDCount, float size, vec3 translation = vec3(0.0f),
                int32_t N = 512, int32_t L = 4000);

            void Update(Camera* camera, float deltaTime);

            /**
             * Sets the distance of a specific level of detail.
             * @param LoD The level of detail to be set in range of (0,LoDCount-1)
             * @param distance The distance where the level of details should begin
             */
            void SetLoDDistance(int32_t LoD, float distance);

            std::vector<OceanNode*> GetRenderList();

            OceanSimulation simulation;

            Texture::Texture2D rippleTexture;
            Texture::Texture2D foamTexture;

            bool enable = true;

            vec3 translation = vec3(0.0f);

            float displacementScale = 4.0f;
            float choppynessScale = 3.0f;

            float tiling = 64.0f;

            float shoreWaveDistanceOffset = 100.0f;
            float shoreWaveDistanceScale = 30.0f;
            float shoreWaveAmplitude = 1.0f;
            float shoreWaveSteepness = 1.5f;
            float shoreWavePower = 4.0f;
            float shoreWaveSpeed = 5.0f;
            float shoreWaveLength = 5.0f;

            bool wireframe = false;

        private:
            void SortNodes(std::vector<OceanNode*>& nodes, vec3 cameraLocation);

            std::vector<OceanNode*> renderList;

            Common::Image<uint8_t> LoDImage;

            std::vector<float> LoDDistances;

            float size;

        };

    }

}


#endif