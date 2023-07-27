#include "ASBuilder.h"

namespace Atlas {

    namespace Graphics {

        BLASDesc ASBuilder::GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer,
            size_t vertexCount, size_t vertexSize, size_t indexCount, size_t indexSize) {

            VkDeviceAddress vertexAddress = vertexBuffer->GetDeviceAddress();
            VkDeviceAddress indexAddress  = indexBuffer->GetDeviceAddress();

            uint32_t triangleCount = indexCount / 3;

            // Describe buffer as array of VertexObj.
            VkAccelerationStructureGeometryTrianglesDataKHR trianglesData = {};
            trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            // Vertex data
            trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
            trianglesData.vertexData.deviceAddress = vertexAddress;
            trianglesData.vertexStride = vertexSize;
            trianglesData.maxVertex = vertexCount;
            // Index data
            trianglesData.indexType = indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            trianglesData.indexData.deviceAddress = indexAddress;

            VkAccelerationStructureGeometryKHR geometry = {};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            geometry.geometry.triangles = trianglesData;

            VkAccelerationStructureBuildRangeInfoKHR buildRange;
            buildRange.firstVertex = 0;
            buildRange.primitiveCount = triangleCount;
            buildRange.primitiveOffset = 0;
            buildRange.transformOffset = 0;

            return BLASDesc {
                .geometries = { geometry },
                .buildRanges = { buildRange }
            };

        }

    }

}