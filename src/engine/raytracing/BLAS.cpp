#include "BLAS.h"

#include "../volume/BVH.h"
#include "../common/Packing.h"

namespace Atlas::RayTracing {

    void BLAS::Build(std::vector<Triangle>& triangles, std::vector<ResourceHandle<Material>>& materials) {

        BuildBuffers(triangles, materials);

        isBvhBuilt = true;

    }

    void BLAS::Build(std::vector<Triangle>& triangles, std::vector<ResourceHandle<Material>>& materials,
        Buffer::VertexBuffer& vertexBuffer, Buffer::IndexBuffer& indexBuffer, std::span<Graphics::ASGeometryRegion> geometryRegions) {

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        bool hardwareRayTracing = device->support.hardwareRayTracing;

        BuildBuffers(triangles, materials);

        if (hardwareRayTracing) {
            Graphics::ASBuilder asBuilder;

            auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(vertexBuffer.buffer, indexBuffer.buffer,
                vertexBuffer.elementCount, vertexBuffer.elementSize, indexBuffer.elementSize, geometryRegions);

            blas = device->CreateBLAS(blasDesc);

            std::vector<uint32_t> triangleOffsets;
            triangleOffsets.reserve(geometryRegions.size());

            for (const auto& region : geometryRegions) {
                auto triangleOffset = uint32_t(region.indexOffset) / 3;
                triangleOffsets.push_back(triangleOffset);
            }

            triangleOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(uint32_t));
            triangleOffsetBuffer.SetSize(triangleOffsets.size(), triangleOffsets.data());

            needsBvhRefresh = true;
        }

        isBvhBuilt = true;

    }

    bool BLAS::IsBuilt() const {

        return isBvhBuilt;

    }

	void BLAS::BuildBuffers(std::vector<Triangle>& triangles, std::vector<ResourceHandle<Material>>& materials) {

		auto device = Graphics::GraphicsDevice::DefaultDevice;
		bool hardwareRayTracing = device->support.hardwareRayTracing;

		this->materials = materials;

		auto triangleCount = triangles.size();

		std::vector<Volume::AABB> aabbs(triangleCount);
		std::vector<Volume::BVHTriangle> bvhTriangles(triangleCount);

		for (size_t i = 0; i < triangleCount; i++) {
			auto min = glm::min(glm::min(triangles[i].v0,
				triangles[i].v1), triangles[i].v2);
			auto max = glm::max(glm::max(triangles[i].v0,
				triangles[i].v1), triangles[i].v2);

			bvhTriangles[i].v0 = triangles[i].v0;
			bvhTriangles[i].v1 = triangles[i].v1;
			bvhTriangles[i].v2 = triangles[i].v2;
			bvhTriangles[i].idx = i;

			aabbs[i] = Volume::AABB(min, max);
		}

		Volume::BVH bvh;
		if (!hardwareRayTracing) {
			// Generate BVH
			bvh = Volume::BVH(aabbs, bvhTriangles, false);

			bvhTriangles.clear();
			bvhTriangles.shrink_to_fit();

			aabbs.clear();
			aabbs.shrink_to_fit();
		}

        auto& data = hardwareRayTracing ? bvhTriangles : bvh.data;

        std::vector<GPUBVHTriangle> gpuBvhTriangles;

        gpuTriangles.reserve(triangleCount);
        if (!hardwareRayTracing)
            gpuBvhTriangles.reserve(triangleCount);

        for (auto& bvhTriangle : data) {

            auto& triangle = triangles[bvhTriangle.idx];

            auto v0v1 = triangle.v1 - triangle.v0;
            auto v0v2 = triangle.v2 - triangle.v0;

            auto uv0uv1 = triangle.uv1 - triangle.uv0;
            auto uv0uv2 = triangle.uv2 - triangle.uv0;

            auto r = 1.0f / (uv0uv1.x * uv0uv2.y - uv0uv2.x * uv0uv1.y);

            auto s = vec3(uv0uv2.y * v0v1.x - uv0uv1.y * v0v2.x,
                uv0uv2.y * v0v1.y - uv0uv1.y * v0v2.y,
                uv0uv2.y * v0v1.z - uv0uv1.y * v0v2.z) * r;

            auto t = vec3(uv0uv1.x * v0v2.x - uv0uv2.x * v0v1.x,
                uv0uv1.x * v0v2.y - uv0uv2.x * v0v1.y,
                uv0uv1.x * v0v2.z - uv0uv2.x * v0v1.z) * r;

            auto normal = glm::normalize(triangle.n0 + triangle.n1 + triangle.n2);

            auto tangent = glm::normalize(s - normal * dot(normal, s));
            auto handedness = (glm::dot(glm::cross(tangent, normal), t) < 0.0f ? 1.0f : -1.0f);

            auto bitangent = handedness * glm::normalize(glm::cross(tangent, normal));

            // Compress data
            auto pn0 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n0, 0.0f));
            auto pn1 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n1, 0.0f));
            auto pn2 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n2, 0.0f));

            auto pt = Common::Packing::PackSignedVector3x10_1x2(vec4(tangent, 0.0f));
            auto pbt = Common::Packing::PackSignedVector3x10_1x2(vec4(bitangent, 0.0f));

            auto puv0 = glm::packHalf2x16(triangle.uv0);
            auto puv1 = glm::packHalf2x16(triangle.uv1);
            auto puv2 = glm::packHalf2x16(triangle.uv2);

            auto pc0 = glm::packUnorm4x8(triangle.color0);
            auto pc1 = glm::packUnorm4x8(triangle.color1);
            auto pc2 = glm::packUnorm4x8(triangle.color2);

            auto cn0 = reinterpret_cast<float&>(pn0);
            auto cn1 = reinterpret_cast<float&>(pn1);
            auto cn2 = reinterpret_cast<float&>(pn2);

            auto ct = reinterpret_cast<float&>(pt);
            auto cbt = reinterpret_cast<float&>(pbt);

            auto cuv0 = reinterpret_cast<float&>(puv0);
            auto cuv1 = reinterpret_cast<float&>(puv1);
            auto cuv2 = reinterpret_cast<float&>(puv2);

            auto cc0 = reinterpret_cast<float&>(pc0);
            auto cc1 = reinterpret_cast<float&>(pc1);
            auto cc2 = reinterpret_cast<float&>(pc2);

            GPUTriangle gpuTriangle;

            gpuTriangle.v0 = vec4(triangle.v0, cn0);
            gpuTriangle.v1 = vec4(triangle.v1, cn1);
            gpuTriangle.v2 = vec4(triangle.v2, cn2);
            gpuTriangle.d0 = vec4(cuv0, cuv1, cuv2, reinterpret_cast<float&>(triangle.materialIdx));
            gpuTriangle.d1 = vec4(ct, cbt, bvhTriangle.endOfNode ? 1.0f : -1.0f, 0.0f);
            gpuTriangle.d2 = vec4(cc0, cc1, cc2, triangle.opacity);

            gpuTriangles.push_back(gpuTriangle);

            if (!hardwareRayTracing) {
                GPUBVHTriangle gpuBvhTriangle;
                gpuBvhTriangle.v0 = vec4(triangle.v0, bvhTriangle.endOfNode ? 1.0f : -1.0f);
                gpuBvhTriangle.v1 = vec4(triangle.v1, reinterpret_cast<float&>(triangle.materialIdx));
                gpuBvhTriangle.v2 = vec4(triangle.v2, triangle.opacity);

                gpuBvhTriangles.push_back(gpuBvhTriangle);
            }

        }

        triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(GPUTriangle));
        triangleBuffer.SetSize(gpuTriangles.size(), gpuTriangles.data());

        if (!hardwareRayTracing) {
            bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(GPUBVHTriangle));
            bvhTriangleBuffer.SetSize(gpuBvhTriangles.size(), gpuBvhTriangles.data());

            gpuBvhTriangles.clear();
            gpuBvhTriangles.shrink_to_fit();

            const auto& nodes = bvh.GetTree();
            std::vector<GPUBVHNode> gpuBvhNodes(nodes.size());
            // Copy to GPU format
            for (size_t i = 0; i < nodes.size(); i++) {
                gpuBvhNodes[i].leftPtr = nodes[i].leftPtr;
                gpuBvhNodes[i].rightPtr = nodes[i].rightPtr;

                gpuBvhNodes[i].leftAABB.min = nodes[i].leftAABB.min;
                gpuBvhNodes[i].leftAABB.max = nodes[i].leftAABB.max;

                gpuBvhNodes[i].rightAABB.min = nodes[i].rightAABB.min;
                gpuBvhNodes[i].rightAABB.max = nodes[i].rightAABB.max;
            }

            blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DedicatedMemoryBit, sizeof(GPUBVHNode));
            blasNodeBuffer.SetSize(gpuBvhNodes.size(), gpuBvhNodes.data());            
        }

	}

}