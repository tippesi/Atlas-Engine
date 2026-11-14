#pragma once

#include "../System.h"

#include "OceanState.h"
#include "OceanNode.h"
#include "OceanSimulation.h"
#include "OceanSurfaceQueries.h"
#include "../resource/Resource.h"
#include "../texture/Texture2D.h"

#include <vector>

namespace Atlas {

    namespace Ocean {

        class Ocean : public OceanNode {

        public:
            Ocean() = default;

            Ocean(int32_t LoDCount, float size, vec3 translation = vec3(0.0f), int32_t N = 512);

            void Update(const CameraComponent& camera, float deltaTime);

            int32_t GetLoDCount() const;

            /**
             * Sets the distance of a specific level of detail.
             * @param LoD The level of detail to be set in range of (0,LoDCount-1)
             * @param distance The distance where the level of details should begin
             */
            void SetLoDDistance(int32_t LoD, float distance);

            std::vector<OceanNode*> GetRenderList();

            OceanSimulation simulation;
            OceanSurfaceQueries surfaceQueries;

            // The normals of the small ripples over the waves and the texture of the foam, both repeated over the surface
            ResourceHandle<Texture::Texture2D> rippleTexture;
            ResourceHandle<Texture::Texture2D> foamTexture;

            vec3 translation = vec3(0.0f);

            vec3 waterBodyColor = vec3(0.1f, 1.0f, 0.7f);
            vec3 scatterColor = vec3(0.3f, 0.7f, 0.6f);
            vec2 waterColorIntensity = vec2(0.1f, 0.2f);

            float displacementScale = 1.0f;
            float choppynessScale = 1.5f;

            float tiling = 32.0f;

            float shoreWaveDistanceOffset = 100.0f;
            float shoreWaveDistanceScale = 30.0f;
            float shoreWaveAmplitude = 1.0f;
            float shoreWaveSteepness = 1.5f;
            float shoreWavePower = 4.0f;
            float shoreWaveSpeed = 5.0f;
            float shoreWaveLength = 5.0f;

            bool wireframe = false;
            bool enable = true;
            bool underwaterShader = true;

            // Bodies float on the waves and the surface can be queried
            bool physics = false;

        private:
            void SortNodes(std::vector<OceanNode*>& nodes, vec3 cameraLocation);

            std::vector<OceanNode*> renderList;

            Common::Image<uint8_t> LoDImage;

            std::vector<float> LoDDistances;

            float size;

        };

    }

}