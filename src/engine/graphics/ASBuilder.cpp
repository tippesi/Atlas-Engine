#include "ASBuilder.h"

#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        BLASDesc ASBuilder::GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer,
            size_t vertexCount, size_t vertexSize, size_t indexSize, std::span<ASGeometryRegion> regions) {

            VkDeviceAddress vertexAddress = vertexBuffer->GetDeviceAddress();
            VkDeviceAddress indexAddress  = indexBuffer->GetDeviceAddress();

            VkAccelerationStructureGeometryTrianglesDataKHR trianglesData = {};
            trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            // Vertex data
            trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            trianglesData.vertexData.deviceAddress = vertexAddress;
            trianglesData.vertexStride = vertexSize;
            trianglesData.maxVertex = vertexCount - 1;
            // Index data
            trianglesData.indexType = indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            trianglesData.indexData.deviceAddress = indexAddress;

            BLASDesc desc;

            for (const auto& region : regions) {
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

        int32_t ASBuilder::BuildBLAS(std::span<Ref<BLAS>> blases, CommandList* commandList) {

            auto device = GraphicsDevice::DefaultDevice;

            size_t compactionCount = 0;

            for(size_t i = 0; i < blases.size(); i++) {
                compactionCount += (blases[i]->buildGeometryInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) ? 1 : 0;
            }

            Ref<QueryPool> queryPool = nullptr;
            // Only on a direct submission we can actually afford to wait for the queries, otherwise we can't
            // afford the compaction if the BVH build is in frame
            if (compactionCount == blases.size() && !commandList) {
                auto queryPoolDesc = QueryPoolDesc{
                    .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                    .queryCount = uint32_t(compactionCount)
                };
                queryPool = device->CreateQueryPool(queryPoolDesc);
            }

            size_t batchSize = 0;
            size_t batchSizeLimit = 256000000;

            std::vector<uint32_t> batchIndices;
            batchIndices.reserve(blases.size());
            for (size_t i = 0; i < blases.size(); i++) {

                batchIndices.push_back(uint32_t(i));
                batchSize += blases[i]->sizesInfo.accelerationStructureSize;

                if (batchSize >= batchSizeLimit || i == blases.size() - 1) {

                    if (queryPool) {
                        queryPool->Reset();
                    }

                    BuildBLASBatch(batchIndices, blases, queryPool, commandList);

                    if (queryPool) {
                        CompactBLASBatch(batchIndices, blases, queryPool, commandList);
                    }

                    batchIndices.clear();
                    batchSize = 0;

                    for (size_t j = 0; j <= i; j++) {
                        blases[j]->isBuilt = true;
                    }

                    // With one commandlist it only makes sense to create one batch per frame
                    if (commandList) {
                        return int32_t(i + 1);
                    }

                }

            }

            return int32_t(blases.size());

        }

        void ASBuilder::UpdateBLAS(std::span<Ref<BLAS>> blases, CommandList* commandList) {

            for (const auto& blas : blases) {
                AE_ASSERT(blas->allowUpdate && blas->isBuilt && "Only built BLASes which allow updates can be updated");
            }

            RecordBLASBuilds(blases, true, commandList);

        }

        Buffer* ASBuilder::BuildTLAS(Ref<Atlas::Graphics::TLAS>& tlas,
            std::span<VkAccelerationStructureInstanceKHR> instances, CommandList* commandList) {

            auto device = GraphicsDevice::DefaultDevice;

            bool immediateSubmission = commandList == nullptr;
            if (!commandList) {
                commandList = device->GetCommandList(GraphicsQueue);

                commandList->BeginCommands();
            }

            BufferDesc desc = {
                .usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                              | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .domain = BufferDomain::Host,
                .data = instances.data(),
                .size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
            };
            if (!instanceBuffer || instanceBuffer->size < desc.size)
                instanceBuffer = device->CreateMultiBuffer(desc);
            else
                instanceBuffer->SetData(instances.data(), 0, desc.size);

            BuildTLAS(tlas, instanceBuffer->GetCurrent()->GetDeviceAddress(), instances.size(), commandList);

            if (immediateSubmission) {
                commandList->EndCommands();

                device->SubmitCommandList(commandList);
            }

            return instanceBuffer->GetCurrent();

        }

        void ASBuilder::BuildTLAS(Ref<TLAS>& tlas, const Ref<Buffer>& instances, size_t instanceCount, CommandList* commandList) {

            BuildTLAS(tlas, instances->GetDeviceAddress(), instanceCount, commandList);

        }

        void ASBuilder::BuildTLAS(Ref<TLAS>& tlas, VkDeviceAddress instanceAddress, size_t instanceCount, CommandList* commandList) {

            auto device = GraphicsDevice::DefaultDevice;

            tlas->Allocate(instanceAddress, uint32_t(instanceCount), false);

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

        }

        void ASBuilder::BuildBLASBatch(const std::span<uint32_t> &batchIndices, std::span<Ref<BLAS>> &blases, 
            Ref<QueryPool>& queryPool, CommandList* commandList) {

            auto device = GraphicsDevice::DefaultDevice;

            bool immediateSubmission = commandList == nullptr;
            if (!commandList) {
                commandList = device->GetCommandList(GraphicsQueue, true);

                commandList->BeginCommands();
            }

            std::vector<Ref<BLAS>> batch;
            batch.reserve(batchIndices.size());
            for (const auto idx : batchIndices)
                batch.push_back(blases[idx]);

            RecordBLASBuilds(batch, false, commandList);

            if (queryPool) {
                std::vector<VkAccelerationStructureKHR> accelerationStructures;
                for (const auto& blas : batch)
                    accelerationStructures.push_back(blas->accelerationStructure);
                vkCmdWriteAccelerationStructuresPropertiesKHR(commandList->commandBuffer, uint32_t(accelerationStructures.size()),
                    accelerationStructures.data(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                    queryPool->pool, 0);
            }

            if (immediateSubmission) {
                commandList->EndCommands();

                device->FlushCommandList(commandList);
            }

        }

        void ASBuilder::RecordBLASBuilds(std::span<const Ref<BLAS>> blases, bool update, CommandList* commandList) {

            if (blases.empty())
                return;

            auto device = GraphicsDevice::DefaultDevice;
            auto alignment = size_t(device->accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);

            auto getScratchSize = [&](const Ref<BLAS>& blas) {
                auto size = size_t(update ? blas->sizesInfo.updateScratchSize : blas->sizesInfo.buildScratchSize);
                return (size + alignment - 1) / alignment * alignment;
            };

            size_t totalScratchSize = 0;
            size_t maxScratchSize = 0;
            for (const auto& blas : blases) {
                totalScratchSize += getScratchSize(blas);
                maxScratchSize = std::max(maxScratchSize, getScratchSize(blas));
            }

            // Builds with their own scratch memory run in parallel, a full scratch buffer waits for the previous builds
            const size_t scratchBudget = 64ull << 20;
            // Builders which are kept, e.g. for animated meshes every frame, reuse their scratch buffer
            auto scratchBufferSize = std::max(maxScratchSize, std::min(totalScratchSize, scratchBudget));
            if (!scratchBuffer || scratchBuffer->size < scratchBufferSize) {
                auto scratchBufferDesc = BufferDesc {
                    .usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .domain = BufferDomain::Device,
                    .size = scratchBufferSize,
                    .alignment = alignment
                };
                scratchBuffer = device->CreateBuffer(scratchBufferDesc);
            }
            auto scratchAddress = scratchBuffer->GetDeviceAddress();

            // Earlier submissions might still build into the scratch buffer or trace the rebuilt structures
            commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

            std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos;
            std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> rangeInfos;
            size_t scratchOffset = 0;

            auto recordBuilds = [&]() {
                if (buildInfos.empty())
                    return;

                commandList->BuildBLASes(buildInfos, rangeInfos);
                commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

                buildInfos.clear();
                rangeInfos.clear();
                scratchOffset = 0;
            };

            for (const auto& blas : blases) {
                auto scratchSize = getScratchSize(blas);
                if (scratchOffset + scratchSize > scratchBuffer->size)
                    recordBuilds();

                if (!update)
                    blas->Allocate();

                auto buildInfo = blas->buildGeometryInfo;
                if (update) {
                    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
                    buildInfo.srcAccelerationStructure = blas->accelerationStructure;
                }
                buildInfo.dstAccelerationStructure = blas->accelerationStructure;
                buildInfo.scratchData.deviceAddress = scratchAddress + scratchOffset;
                scratchOffset += scratchSize;

                buildInfos.push_back(buildInfo);
                rangeInfos.push_back(blas->rangeInfo);
            }

            recordBuilds();

        }

        void ASBuilder::CompactBLASBatch(const std::span<uint32_t>& batchIndices,
            std::span<Ref<BLAS>>& blases, Ref<QueryPool>& queryPool, CommandList* commandList) {

            auto device = GraphicsDevice::DefaultDevice;

            bool immediateSubmission = commandList == nullptr;
            if (!commandList) {
                commandList = device->GetCommandList(GraphicsQueue, true);

                commandList->BeginCommands();
            }

            std::vector<size_t> compactSizes(batchIndices.size());
            queryPool->GetResult(0, uint32_t(batchIndices.size()), batchIndices.size() * sizeof(size_t),
                compactSizes.data(), sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT);

            uint32_t poolCounter = 0;
            for (const auto idx : batchIndices) {
                auto& blas = blases[idx];
                auto compactedSize = compactSizes[poolCounter++];

                BLASDesc desc;
                desc.geometries = blas->geometries;
                desc.buildRanges = blas->buildRanges;
                desc.flags = blas->flags;

                auto compactedBlas = device->CreateBLAS(desc);

                compactedBlas->sizesInfo.accelerationStructureSize = compactedSize;

                compactedBlas->Allocate();

                compactedBlas->sizesInfo.accelerationStructureSize = blas->sizesInfo.accelerationStructureSize;

                // Copy generated BLAS into compacted one
                VkCopyAccelerationStructureInfoKHR copyInfo = {};
                copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
                copyInfo.src  = blas->accelerationStructure;
                copyInfo.dst  = compactedBlas->accelerationStructure;
                copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                
                vkCmdCopyAccelerationStructureKHR(commandList->commandBuffer, &copyInfo);

                blases[idx] = compactedBlas;

            }

            commandList->MemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

            if (immediateSubmission) {
                commandList->EndCommands();

                device->FlushCommandList(commandList);
            }

        }

    }

}