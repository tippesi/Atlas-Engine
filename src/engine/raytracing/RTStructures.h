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

    // One skinned BLAS in a skinning pass, the buffers are accessed through their device addresses
    struct GPUSkinJob {
        VkDeviceAddress skinVertices = 0;
        VkDeviceAddress triangleVertices = 0;
        VkDeviceAddress target = 0;
        VkDeviceAddress bvhTriangles = 0;
        uint32_t firstGroup = 0;
        uint32_t elementCount = 0;
        uint32_t jointOffset = 0;
        uint32_t padding0 = 0;
    };

    struct GPUSkinVertex {
        // The packed normal is stored in w
        vec4 position;
        uint32_t joints01 = 0;
        uint32_t joints23 = 0;
        uint32_t weights = 0;
        uint32_t padding = 0;
    };

    // Undeformed vertex of a mesh for the deformation pass, has to match raytracer/deform.csh
    struct GPUDeformVertex {
        // The packed normal is stored in w
        vec4 position;
        // Packed with the handedness in w
        uint32_t tangent = 0;
        uint32_t texCoord = 0;
        uint32_t color = 0;
        uint32_t weights = 0;
        uint32_t joints01 = 0;
        uint32_t joints23 = 0;
        uint32_t padding0 = 0;
        uint32_t padding1 = 0;
    };

    // One deformed vertex range of an instance, the buffers are accessed through their device addresses
    struct GPUDeformJob {
        mat3x4 matrix;
        VkDeviceAddress sourceVertices = 0;
        VkDeviceAddress positions = 0;
        VkDeviceAddress normals = 0;
        uint32_t firstGroup = 0;
        uint32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
        uint32_t jointOffset = 0;
        float windBendScale = 0.0f;
        float windWiggleScale = 0.0f;
        float windTextureLod = 0.0f;
        uint32_t materialGraphOffset = 0;
        uint32_t invertUVs = 0;
        float uvTiling = 1.0f;
        vec2 uvAnimation = vec2(0.0f);
        uint32_t padding0 = 0;
        uint32_t padding1 = 0;
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

    struct alignas(16) GPUMaterial {
        int32_t ID = 0;

        vec3 baseColor = vec3(1.0f);
        vec3 emissiveColor = vec3(0.0f);

        float opacity = 1.0f;

        float roughness = 1.0f;
        float metalness = 0.0f;
        float ao = 1.0f;

        float reflectance = 0.5f;

        float normalScale = 1.0f;

        float tiling = 1.0f;
        float terrainTiling = 1.0f;

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
        int32_t terrainNormalTexture = -1;
        // RGB packed as 10:10:10
        uint32_t transmissionColor = 0;

        // Hit group of a material graph in ray tracing pipelines (see RayTracingWorld::hitRecords), 0 without one
        uint32_t graphHitGroup = 0;
        uint32_t graphParameterOffset = 0;
        int32_t padding0 = 0;
        int32_t padding1 = 0;
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

        // Normals and tangents of a deformed instance and the index buffer of its mesh, the lowest bit of the
        // index address marks 16 bit indices
        VkDeviceAddress deformedVertices = 0;
        VkDeviceAddress meshIndices = 0;
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
