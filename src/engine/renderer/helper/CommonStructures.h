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

            float cascadeBlendDistance;

            int cascadeCount;

            float aligment0;
            float aligment1;

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

            float alignment;

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

    }

}