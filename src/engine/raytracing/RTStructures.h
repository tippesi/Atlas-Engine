#pragma once

#include "System.h"

#include "graphics/BLAS.h"

namespace Atlas {

    enum InstanceCullMasks {
        MaskAll = 1 << 7,
        MaskShadow = 1 << 6
    };

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
        int32_t ID = 0;

        vec3 baseColor = vec3(1.0f);
        vec3 emissiveColor = vec3(0.0f);

        float opacity = 1.0f;

        float roughness = 1.0f;
        float metalness = 0.0f;
        float ao = 1.0f;

        float reflectance = 0.5f;

        float normalScale = 1.0f;

        int32_t invertUVs = 0;
        int32_t twoSided = 0;
        int32_t cullBackFaces = 0;
        int32_t useVertexColors = 0;

        int32_t baseColorTexture = -1;
        int32_t opacityTexture = -1;
        int32_t normalTexture = -1;
        int32_t roughnessTexture = -1;
        int32_t metalnessTexture = -1;
        int32_t aoTexture = -1;
        int32_t emissiveTexture = -1;
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
        uint32_t mask;
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