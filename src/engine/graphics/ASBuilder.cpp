#include "ASBuilder.h"

#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        BLASDesc ASBuilder::GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer,
            size_t vertexCount, size_t vertexSize, size_t indexCount, size_t indexSize) {

            VkDeviceAddress vertexAddress = vertexBuffer->GetDeviceAddress();
            VkDeviceAddress indexAddress  = indexBuffer->GetDeviceAddress();

            uint32_t triangleCount = indexCount / 3;

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

        void ASBuilder::BuildBLAS(std::vector<Ref<BLAS>> &blases) {

            auto device = GraphicsDevice::DefaultDevice;

            size_t totalSize = 0;
            size_t maxScratchSize = 0;

            for(size_t i = 0; i < blases.size(); i++) {
                totalSize += blases[i]->sizesInfo.accelerationStructureSize;
                maxScratchSize = std::max(maxScratchSize, size_t(blases[i]->sizesInfo.buildScratchSize));
                // nbCompactions += hasFlag(buildAs[idx].buildInfo.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
            }

            auto scratchBufferDesc = BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = BufferDomain::Device,
                .size = maxScratchSize
            };
            auto scratchBuffer = device->CreateBuffer(scratchBufferDesc);

            size_t batchSize = 0;
            size_t batchSizeLimit = 256000000;

            std::vector<uint32_t> batchIndices;
            for (size_t i = 0; i < blases.size(); i++) {

                batchIndices.push_back(uint32_t(i));
                batchSize += blases[i]->sizesInfo.accelerationStructureSize;

                if (batchSize >= batchSizeLimit || i == blases.size() - 1) {
                    BuildBLASBatch(batchIndices, blases, scratchBuffer);

                    batchIndices.clear();
                    batchSize = 0;

                }

            }

        }

        Ref<Buffer> ASBuilder::BuildTLAS(Ref<Atlas::Graphics::TLAS> &tlas,
            std::vector<VkAccelerationStructureInstanceKHR> &instances) {

            auto device = GraphicsDevice::DefaultDevice;

            auto commandList = device->GetCommandList(GraphicsQueue, true);

            BufferDesc desc = {
                .usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                              | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .domain = BufferDomain::Host,
                .data = instances.data(),
                .size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
            };
            auto instanceBuffer = device->CreateBuffer(desc);

            tlas->Allocate(instanceBuffer->GetDeviceAddress(), uint32_t(instances.size()), false);

            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

            auto scratchBufferDesc = BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = BufferDomain::Device,
                .size = tlas->sizesInfo.buildScratchSize
            };
            auto scratchBuffer = device->CreateBuffer(scratchBufferDesc);

            auto buildInfo = tlas->buildGeometryInfo;
            buildInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
            buildInfo.dstAccelerationStructure = tlas->accelerationStructure;
            buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

            commandList->BuildTLAS(tlas, buildInfo);

            device->FlushCommandList(commandList);

            return instanceBuffer;

        }

        void ASBuilder::BuildBLASBatch(const std::vector<uint32_t> &batchIndices,
            std::vector<Ref<BLAS>> &blases, Ref<Buffer>& scratchBuffer) {

            auto device = GraphicsDevice::DefaultDevice;

            auto commandList = device->GetCommandList(GraphicsQueue, true);

            VkDeviceAddress scratchAddress = scratchBuffer->GetDeviceAddress();

            for (const auto idx : batchIndices) {
                auto& blas = blases[idx];

                auto buildInfo = blas->buildGeometryInfo;
                buildInfo.dstAccelerationStructure = blas->accelerationStructure;
                buildInfo.scratchData.deviceAddress = scratchAddress;

                commandList->BuildBLAS(blas, buildInfo);

                commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
            }

            device->FlushCommandList(commandList);

        }

    }

}