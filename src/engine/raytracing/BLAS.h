#pragma once

#include <vector>

#include "Material.h"
#include "resource/Resource.h"
#include "buffer/Buffer.h"
#include "buffer/IndexBuffer.h"
#include "buffer/VertexBuffer.h"
#include "graphics/BLAS.h"
#include "graphics/ASBuilder.h"
#include "RTStructures.h"

namespace Atlas::RayTracing {

    class BLAS {

    public:
        struct Triangle {
            vec3 v0;
            vec3 v1;
            vec3 v2;

            vec3 n0;
            vec3 n1;
            vec3 n2;

            vec2 uv0;
            vec2 uv1;
            vec2 uv2;

            vec4 color0;
            vec4 color1;
            vec4 color2;

            int32_t materialIdx = 0;
            float opacity = -1.0f;
        };

        BLAS() = default;

        void Build(std::vector<Triangle>& triangles, std::vector<ResourceHandle<Material>>& materials);

        void Build(std::vector<Triangle>& triangles, std::vector<ResourceHandle<Material>>& materials,
            Buffer::VertexBuffer& vertexBuffer, Buffer::IndexBuffer& indexBuffer, std::span<Graphics::ASGeometryRegion> geometryRegions);

        bool IsBuilt() const;

        std::vector<ResourceHandle<Material>> materials;

        std::vector<GPUTriangle> gpuTriangles;

        Buffer::Buffer blasNodeBuffer;
        Buffer::Buffer triangleBuffer;
        Buffer::Buffer bvhTriangleBuffer;
        Buffer::Buffer triangleOffsetBuffer;

        Ref<Graphics::BLAS> blas = nullptr;

        std::atomic_bool needsBvhRefresh{ false };
        std::atomic_bool isBvhBuilt { false };

    private:
        void BuildBuffers(std::vector<Triangle>& triangles, std::vector<ResourceHandle<Material>>& materials);

	};

}