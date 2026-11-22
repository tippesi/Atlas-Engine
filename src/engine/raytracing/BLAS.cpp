#include "BLAS.h"

#include "../volume/BVH.h"
#include "../common/Packing.h"

#include <glm/gtc/packing.hpp>

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

            auto blasDesc = asBuilder.GetBLASDescForTriangleGeometry(vertexBuffer.buffer->GetDeviceAddress() + vertexBuffer.offset,
                indexBuffer.buffer->GetDeviceAddress() + indexBuffer.offset, vertexBuffer.elementCount, vertexBuffer.elementSize,
                indexBuffer.elementSize, geometryRegions);
            if (isDynamic) {
                blasDesc.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
                blasDesc.isDynamic = true;
                blasDesc.allowUpdate = allowUpdate;
            }

            blas = device->CreateBLAS(blasDesc);

            // The triangles of the geometries follow each other, the regions of levels of detail start behind level 0
            std::vector<uint32_t> triangleOffsets;
            triangleOffsets.reserve(geometryRegions.size());

            uint32_t triangleOffset = 0;
            for (const auto& region : geometryRegions) {
                triangleOffsets.push_back(triangleOffset);
                triangleOffset += uint32_t(region.indexCount) / 3;
            }

            triangleOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t));
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
        triangleIndices.reserve(triangleCount);
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

            AE_ASSERT(triangle.materialIdx >= 0 && triangle.materialIdx <= 0xFFFF && "Triangles hold 16 bit material indices");

            // The shaders rebuild the bitangent from the handedness, see UnpackTriangle in raytracer/common.hsh
            GPUTriangle gpuTriangle;
            gpuTriangle.normals[0] = uint32_t(Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n0, 0.0f)));
            gpuTriangle.normals[1] = uint32_t(Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n1, 0.0f)));
            gpuTriangle.normals[2] = uint32_t(Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n2, 0.0f)));
            gpuTriangle.tangent = uint32_t(Common::Packing::PackSignedVector3x10_1x2(vec4(tangent, handedness)));
            gpuTriangle.texCoords[0] = glm::packHalf2x16(triangle.uv0);
            gpuTriangle.texCoords[1] = glm::packHalf2x16(triangle.uv1);
            gpuTriangle.texCoords[2] = glm::packHalf2x16(triangle.uv2);
            gpuTriangle.materialOpacity = uint32_t(triangle.materialIdx) | (uint32_t(glm::packHalf1x16(triangle.opacity)) << 16u);
            gpuTriangle.colors[0] = glm::packUnorm4x8(triangle.color0);
            gpuTriangle.colors[1] = glm::packUnorm4x8(triangle.color1);
            gpuTriangle.colors[2] = glm::packUnorm4x8(triangle.color2);

            gpuTriangles.push_back(gpuTriangle);
            triangleIndices.push_back(uint32_t(bvhTriangle.idx));

            if (!hardwareRayTracing) {
                GPUBVHTriangle gpuBvhTriangle;
                gpuBvhTriangle.v0 = vec4(triangle.v0, bvhTriangle.endOfNode ? 1.0f : -1.0f);
                gpuBvhTriangle.v1 = vec4(triangle.v1, 0.0f);
                gpuBvhTriangle.v2 = vec4(triangle.v2, triangle.opacity);

                gpuBvhTriangles.push_back(gpuBvhTriangle);
            }

        }

        // Software ray tracing copies the buffers into the BLASes of deformed instances
        auto copyUsage = hardwareRayTracing ? 0u : uint32_t(Buffer::BufferUsageBits::MemoryTransfersBit);

        triangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DeviceAddressBit |
            copyUsage, sizeof(GPUTriangle));
        triangleBuffer.SetSize(gpuTriangles.size(), gpuTriangles.data());

        if (!hardwareRayTracing) {
            bvhTriangleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::DeviceAddressBit |
                copyUsage, sizeof(GPUBVHTriangle));
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

            nodeParents = std::vector<int32_t>(gpuBvhNodes.size(), -1);
            for (size_t i = 0; i < gpuBvhNodes.size(); i++) {
                if (gpuBvhNodes[i].leftPtr >= 0)
                    nodeParents[gpuBvhNodes[i].leftPtr] = int32_t(i) * 2;
                if (gpuBvhNodes[i].rightPtr >= 0)
                    nodeParents[gpuBvhNodes[i].rightPtr] = int32_t(i) * 2 + 1;
            }

            blasNodeBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit | copyUsage, sizeof(GPUBVHNode));
            blasNodeBuffer.SetSize(gpuBvhNodes.size(), gpuBvhNodes.data());            
        }

	}

}