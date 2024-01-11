#pragma once

#include <System.h>

#include "../graphics/BLAS.h"

namespace Atlas {

    struct GPUTriangle {
        vec4 v0;
        vec4 v1;
        vec4 v2;
        vec4 d0;
        vec4 d1;
        vec4 d2;
    };

    struct GPUBVHTriangle {
        vec4 v0;
        vec4 v1;
        vec4 v2;
    };

    struct GPUTextureLevel {
        int32_t layer = 0;

        int32_t x = 0;
        int32_t y = 0;

        int32_t width = 0;
        int32_t height = 0;

        int32_t valid = -1;
    };

    struct GPUTexture {
        GPUTextureLevel level0;
        GPUTextureLevel level1;
        GPUTextureLevel level2;
        GPUTextureLevel level3;
        GPUTextureLevel level4;

        int32_t valid = -1;
    };

    struct GPUMaterial {
        int32_t ID;

        vec3 baseColor;
        vec3 emissiveColor;

        float opacity;

        float roughness;
        float metalness;
        float ao;

        float reflectance;

        float normalScale;

        int32_t invertUVs;
        int32_t twoSided;
        int32_t useVertexColors;

        int32_t baseColorTexture = -1;
        int32_t opacityTexture = -1;
        int32_t normalTexture = -1;
        int32_t roughnessTexture = -1;
        int32_t metalnessTexture = -1;
        int32_t aoTexture = -1;
    };

    struct GPUAABB {
        vec3 min;
        vec3 max;
    };

    struct GPUBVHInstance {
        mat3x4 inverseMatrix;

        int32_t meshOffset;
        int32_t materialOffset;

        int32_t nextInstance;
        int32_t padding1;
    };

    struct GPUBVHNode {
        GPUAABB leftAABB;
        GPUAABB rightAABB;
        int32_t leftPtr = 0;
        int32_t rightPtr = 0;

        int32_t padding0;
        int32_t padding1;
    };

    struct GPULight {
        vec4 P;
        vec4 N;
        vec4 color;
        vec4 data;
    };

}