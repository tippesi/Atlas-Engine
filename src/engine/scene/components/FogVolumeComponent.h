#pragma once

#include "../../System.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            enum class FogVolumeShape {
                Box = 0,
                Ellipsoid
            };

            // Fog in the unit box [-1, 1] of the entity transform, rendered as part of the froxel fog
            class FogVolumeComponent {

            public:
                FogVolumeComponent() = default;

                bool enable = true;

                FogVolumeShape shape = FogVolumeShape::Box;

                // Extinction per world unit
                float density = 0.05f;
                vec3 scatteringColor = vec3(1.0f);
                // Scales the light the volume scatters towards the camera without changing its extinction
                float scatteringFactor = 1.0f;
                // Phase function from -1 to 1 with the sign of the scene fog: negative values scatter towards the light
                // (bright halos around the sun and light shafts), positive ones back to it. 0 scatters evenly
                float scatteringAnisotropy = -0.6f;
                vec3 emissiveColor = vec3(0.0f);
                // Factor of the sky light, enclosed spaces need a lower one
                float ambientFactor = 0.4f;
                // 0 is off and 1 physically based
                float selfShadowing = 1.0f;

                // Fraction of the unit shape
                float edgeFade = 0.25f;
                float heightFalloff = 0.0f;

                float noiseAmount = 0.8f;
                // Size of the largest structures in world units
                float noiseScale = 15.0f;
                float noiseCoverage = 0.5f;
                float noiseSharpness = 0.5f;
                int32_t noiseOctaves = 3;
                // Fraction of the wind speed
                float windInfluence = 0.1f;

                // Maximum camera distance, 0 keeps the volume at any distance
                float fadeDistance = 0.0f;

            };

        }

    }

}
