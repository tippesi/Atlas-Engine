#include "ASBuilder.h"

#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        BLASDesc ASBuilder::GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer,
            size_t vertexCount, size_t vertexSize, size_t indexSize, std::vector<ASGeometryRegion> regions) {

            VkDeviceAddress vertexAddress = vertexBuffer->GetDeviceAddress();
            VkDeviceAddress indexAddress  = indexBuffer->GetDeviceAddress();

            VkAccelerationStructureGeometryTrianglesDataKHR trianglesData = {};
            trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            // Vertex data
            trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            trianglesData.vertexData.deviceAddress = vertexAddress;
            trianglesData.vertexStride = vertexSize;
            trianglesData.maxVertex = vertexCount;
            // Index data
            trianglesData.indexType = indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            trianglesData.indexData.deviceAddress = indexAddress;

            BLASDesc desc;

            for (auto& region : regions) {
                VkAccelerationStructureGeometryKHR geometry = {};
                geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                geometry.flags = region.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR 
                    : VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
                geometry.geometry.triangles = trianglesData;

                uint32_t triangleCount = uint32_t(region.indexCount / 3);

                VkAccelerationStructureBuildRangeInfoKHR buildRange;
                buildRange.firstVertex = 0;
                buildRange.primitiveCount = triangleCount;
                buildRange.primitiveOffset = uint32_t(region.indexOffset * indexSize);
                buildRange.transformOffset = 0;

                desc.geometries.push_back(geometry);
                desc.buildRanges.push_back(buildRange);
            }

            return desc;

        }

        void ASBuilder::BuildBLAS(std::vector<Ref<BLAS>> &blases) {

            auto device = GraphicsDevice::DefaultDevice;

            size_t maxScratchSize = 0;
            size_t compactionCount = 0;

            for(size_t i = 0; i < blases.size(); i++) {
                maxScratchSize = std::max(maxScratchSize, size_t(blases[i]->sizesInfo.buildScratchSize));
                compactionCount += (blases[i]->buildGeometryInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) ? 1 : 0;
            }

            auto scratchBufferDesc = BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .domain = BufferDomain::Device,
                .size = maxScratchSize,
                .alignment = device->accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment
            };
            auto scratchBuffer = device->CreateBuffer(scratchBufferDesc);

            Ref<QueryPool> queryPool = nullptr;
            if (compactionCount == blases.size()) {
                auto queryPoolDesc = QueryPoolDesc{
                    .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                    .queryCount = uint32_t(compactionCount)
                };
                queryPool = device->CreateQueryPool(queryPoolDesc);
            }

            size_t batchSize = 0;
            size_t batchSizeLimit = 256000000;

            std::vector<uint32_t> batchIndices;
            for (size_t i = 0; i < blases.size(); i++) {

                batchIndices.push_back(uint32_t(i));
                batchSize += blases[i]->sizesInfo.accelerationStructureSize;

                if (batchSize >= batchSizeLimit || i == blases.size() - 1) {

                    if (queryPool) {
                        queryPool->Reset();
                    }

                    BuildBLASBatch(batchIndices, blases, scratchBuffer, queryPool);

                    if (queryPool) {
                        CompactBLASBatch(batchIndices, blases, queryPool);
                    }

                    batchIndices.clear();
                    batchSize = 0;

                }

            }

        }

        Ref<Buffer> ASBuilder::BuildTLAS(Ref<Atlas::Graphics::TLAS> &tlas,
            std::vector<VkAccelerationStructureInstanceKHR> &instances) {

            auto device = GraphicsDevice::DefaultDevice;

            auto commandList = device->GetCommandList(GraphicsQueue);

            commandList->BeginCommands();

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
                .size = tlas->sizesInfo.buildScratchSize,
                .alignment = device->accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment
            };
            auto scratchBuffer = device->CreateBuffer(scratchBufferDesc);

            auto buildInfo = tlas->buildGeometryInfo;
            buildInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
            buildInfo.dstAccelerationStructure = tlas->accelerationStructure;
            buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

            commandList->BuildTLAS(tlas, buildInfo);

            commandList->EndCommands();

            device->SubmitCommandList(commandList);

            return instanceBuffer;

        }

        void ASBuilder::BuildBLASBatch(const std::vector<uint32_t> &batchIndices,
            std::vector<Ref<BLAS>> &blases, Ref<Buffer>& scratchBuffer, Ref<QueryPool>& queryPool) {

            auto device = GraphicsDevice::DefaultDevice;

            auto commandList = device->GetCommandList(GraphicsQueue, true);

            commandList->BeginCommands();

            VkDeviceAddress scratchAddress = scratchBuffer->GetDeviceAddress();

            for (const auto idx : batchIndices) {
                auto& blas = blases[idx];

                blas->Allocate();

                auto buildInfo = blas->buildGeometryInfo;
                buildInfo.dstAccelerationStructure = blas->accelerationStructure;
                buildInfo.scratchData.deviceAddress = scratchAddress;

                commandList->BuildBLAS(blas, buildInfo);

                commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

                if (queryPool) {
                    vkCmdWriteAccelerationStructuresPropertiesKHR(commandList->commandBuffer, 1,
                        &blas->accelerationStructure, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                        queryPool->pool, idx);
                }
            }

            commandList->EndCommands();

            device->FlushCommandList(commandList);

        }

        void ASBuilder::CompactBLASBatch(const std::vector<uint32_t>& batchIndices,
            std::vector<Ref<BLAS>>& blases, Ref<QueryPool>& queryPool) {

            auto device = GraphicsDevice::DefaultDevice;

            auto commandList = device->GetCommandList(GraphicsQueue, true);

            commandList->BeginCommands();

            std::vector<size_t> compactSizes(blases.size());
            queryPool->GetResult(0, uint32_t(blases.size()), blases.size() * sizeof(size_t),
                compactSizes.data(), sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT);

            for (const auto idx : batchIndices) {
                auto& blas = blases[idx];
                auto compactedSize = compactSizes[idx];

                BLASDesc desc;
                desc.geometries = blas->geometries;
                desc.buildRanges = blas->buildRanges;
                desc.flags = blas->flags;

                auto compactedBlas = device->CreateBLAS(desc);

                compactedBlas->sizesInfo.accelerationStructureSize = compactedSize;

                compactedBlas->Allocate();

                // Copy generated BLAS into compacted one
                VkCopyAccelerationStructureInfoKHR copyInfo = {};
                copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
                copyInfo.src  = blas->accelerationStructure;
                copyInfo.dst  = compactedBlas->accelerationStructure;
                copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                
                vkCmdCopyAccelerationStructureKHR(commandList->commandBuffer, &copyInfo);

                blases[idx] = compactedBlas;

            }

            commandList->EndCommands();

            device->FlushCommandList(commandList);

        }

    }

}