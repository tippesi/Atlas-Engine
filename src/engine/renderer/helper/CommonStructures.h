#pragma once

#include "../../System.h"

namespace Atlas {

    namespace Renderer {

        struct alignas(16) Cascade {
            float distance;
            float texelSize;
            float aligment0;
            float aligment1;
            mat4 cascadeSpace;
        };

        struct alignas(16) Shadow {
            float distance;
            float bias;

            float edgeSoftness;
            float cascadeBlendDistance;

            int cascadeCount;

            float mapIdx;

            vec2 resolution;

            Cascade cascades[6];
        };

        struct alignas(16) Light {
            vec4 location;
            vec4 direction;

            vec4 color;
            float intensity;

            float scatteringFactor;

            float radius;
            float attenuation;

            Shadow shadow;
        };

        struct alignas(16) CloudShadow {
            mat4 vMatrix;
            mat4 pMatrix;

            mat4 ivMatrix;
            mat4 ipMatrix;
        };

        struct alignas(16) Fog {
            vec4 extinctionCoefficient;
            float density;
            float heightFalloff;
            float height;
            float scatteringAnisotropy;
            float scatteringFactor;
            float extinctionFactor;
            float ambientFactor;
        };

        struct alignas(16) PackedMaterial {

            int32_t baseColor;
            int32_t emissiveColor;
            int32_t transmissionColor;

            uint32_t emissiveIntensityTiling;

            int32_t data0;
            int32_t data1;
            int32_t data2;

            int32_t features;

        };

        struct alignas(16) GlobalUniforms {
            vec4 frustumPlanes[6];
            mat4 vMatrix;
            mat4 pMatrix;
            mat4 ivMatrix;
            mat4 ipMatrix;
            mat4 pvMatrixLast;
            mat4 pvMatrixCurrent;
            mat4 ipvMatrixLast;
            mat4 ipvMatrixCurrent;
            vec2 jitterLast;
            vec2 jitterCurrent;
            vec4 cameraLocation;
            vec4 cameraDirection;
            vec4 cameraUp;
            vec4 cameraRight;
            vec4 planetCenter;
            vec2 windDir;
            float windSpeed;
            float planetRadius;
            float time;
            float deltaTime;
            uint32_t frameCount;
            float mipLodBias;
        };

        struct alignas(16) DDGICascade {
            vec4 volumeMin;
            vec4 volumeMax;
            vec4 cellSize;
            ivec4 offsetDifference;
        };

        struct alignas(16) DDGIUniforms {

            DDGICascade cascades[MAX_IRRADIANCE_VOLUME_CASCADES];

            vec4 volumeCenter;
            ivec4 volumeProbeCount;
            int32_t cascadeCount;

            float volumeBias;

            int32_t volumeIrradianceRes;
            int32_t volumeMomentsRes;

            uint32_t rayCount;
            uint32_t inactiveRayCount;

            float hysteresis;

            float volumeGamma;
            float volumeStrength;

            float depthSharpness;
            int optimizeProbes;

            int32_t volumeEnabled;
        };

    }

}