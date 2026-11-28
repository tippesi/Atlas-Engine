#include "GeometryPool.h"

#include "../graphics/Format.h"
#include "../graphics/GraphicsDevice.h"
#include "../graphics/MemoryTransferManager.h"

#include <algorithm>

namespace Atlas {

    namespace Buffer {

        std::mutex GeometryPool::mutex;
        std::vector<Ref<GeometryPool::VertexChunk>> GeometryPool::vertexChunks;
        std::vector<Ref<GeometryPool::IndexChunk>> GeometryPool::indexChunks;
        std::vector<GeometryPool::PendingFree> GeometryPool::pendingFrees;
        bool GeometryPool::shutdown = false;

        GeometryPool::Block::~Block() {

            vmaClearVirtualBlock(block);
            vmaDestroyVirtualBlock(block);

        }

        Ref<GeometryPool::Range> GeometryPool::Allocate(const std::vector<VkFormat>& vertexFormats, VkIndexType indexType,
            size_t vertexCount, size_t indexCount) {

            std::scoped_lock lock(mutex);

            // An engine started again after a shutdown delays the frees again
            shutdown = false;
            ReleasePendingFrees(false);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (device->support.hardwareRayTracing)
                usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            auto createBlock = [](Block& block, size_t capacity) {
                block.capacity = capacity;
                VmaVirtualBlockCreateInfo blockInfo = { .size = capacity };
                VK_CHECK(vmaCreateVirtualBlock(&blockInfo, &block.block))
            };

            Ref<Range> rangeRef(new Range(), Release);
            auto& range = *rangeRef;

            // Fuller chunks first, so the emptier ones run empty while meshes are streamed and can be released
            auto byUsage = [](auto& chunks) {
                std::stable_sort(chunks.begin(), chunks.end(), [](const auto& chunk0, const auto& chunk1) {
                    return GetUsedCount(*chunk0) > GetUsedCount(*chunk1);
                    });
            };
            byUsage(vertexChunks);
            byUsage(indexChunks);

            size_t previousCapacity = 0;
            for (const auto& chunk : vertexChunks) {
                if (chunk->formats != vertexFormats)
                    continue;
                previousCapacity = std::max(previousCapacity, chunk->capacity);
                if (AllocateInBlock(*chunk, vertexCount, 1, range.vertexAllocation, range.firstVertex)) {
                    range.vertexChunk = chunk;
                    break;
                }
            }
            if (!range.vertexChunk) {
                auto chunk = CreateRef<VertexChunk>();
                chunk->formats = vertexFormats;
                createBlock(*chunk, GetChunkCapacity(previousCapacity, 1 << 16, 1 << 20, vertexCount));
                for (auto format : vertexFormats) {
                    Graphics::BufferDesc desc = {
                        .usageFlags = usage | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        .domain = Graphics::BufferDomain::Device,
                        .size = chunk->capacity * Graphics::GetFormatSize(format),
                        .sharedWithTransferQueue = true
                    };
                    chunk->buffers.push_back(device->CreateBuffer(desc));
                }
                vertexChunks.push_back(chunk);

                auto allocated = AllocateInBlock(*chunk, vertexCount, 1, range.vertexAllocation, range.firstVertex);
                AE_ASSERT(allocated && "A new chunk has to fit the vertices");
                range.vertexChunk = chunk;
            }

            // Ray tracing reads 16 bit indices in pairs from their address, which needs 4 byte alignment
            size_t indexAlignment = indexType == VK_INDEX_TYPE_UINT16 ? 2 : 1;
            previousCapacity = 0;
            for (const auto& chunk : indexChunks) {
                if (chunk->type != indexType)
                    continue;
                previousCapacity = std::max(previousCapacity, chunk->capacity);
                if (AllocateInBlock(*chunk, indexCount, indexAlignment, range.indexAllocation, range.firstIndex)) {
                    range.indexChunk = chunk;
                    break;
                }
            }
            if (!range.indexChunk) {
                auto chunk = CreateRef<IndexChunk>();
                chunk->type = indexType;
                createBlock(*chunk, GetChunkCapacity(previousCapacity, 1 << 18, 1 << 22, indexCount));
                Graphics::BufferDesc desc = {
                    .usageFlags = usage | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    .domain = Graphics::BufferDomain::Device,
                    .size = chunk->capacity * (indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4),
                    .sharedWithTransferQueue = true
                };
                chunk->buffer = device->CreateBuffer(desc);
                indexChunks.push_back(chunk);

                auto allocated = AllocateInBlock(*chunk, indexCount, indexAlignment, range.indexAllocation, range.firstIndex);
                AE_ASSERT(allocated && "A new chunk has to fit the indices");
                range.indexChunk = chunk;
            }

            return rangeRef;

        }

        void GeometryPool::Release(Range* range) {

            std::scoped_lock lock(mutex);

            // Meshes which outlive the engine don't have a device anymore
            if (shutdown)
                Free(range);
            else
                pendingFrees.push_back({ range, Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex() });

        }

        void GeometryPool::Upload(const Range& range, const std::vector<std::pair<const void*, size_t>>& vertexData,
            const void* indexData, size_t indexCount) {

            const auto& vertexChunk = *range.vertexChunk;
            const auto& indexChunk = *range.indexChunk;
            AE_ASSERT(vertexData.size() == vertexChunk.buffers.size() && "Each vertex buffer of the chunk needs data");

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            Graphics::MemoryTransferManager transferManager(device, device->memoryManager);

            transferManager.BeginMultiTransfer(Graphics::TransferQueue);
            for (size_t i = 0; i < vertexData.size(); i++) {
                auto elementSize = Graphics::GetFormatSize(vertexChunk.formats[i]);
                vertexChunk.buffers[i]->SetData(const_cast<void*>(vertexData[i].first), range.firstVertex * elementSize,
                    vertexData[i].second * elementSize, &transferManager);
            }
            if (indexCount) {
                auto indexSize = indexChunk.type == VK_INDEX_TYPE_UINT16 ? 2 : 4;
                indexChunk.buffer->SetData(const_cast<void*>(indexData), range.firstIndex * indexSize,
                    indexCount * indexSize, &transferManager);
            }
            transferManager.EndMultiTransfer();

        }

        void GeometryPool::Update() {

            std::scoped_lock lock(mutex);

            ReleasePendingFrees(false);

        }

        void GeometryPool::Shutdown() {

            std::scoped_lock lock(mutex);

            ReleasePendingFrees(true);
            vertexChunks.clear();
            indexChunks.clear();
            shutdown = true;

        }

        size_t GeometryPool::GetChunkCapacity(size_t previousCapacity, size_t minCapacity, size_t maxCapacity, size_t count) {

            return std::max(std::clamp(2 * previousCapacity, minCapacity, maxCapacity), count);

        }

        size_t GeometryPool::GetUsedCount(const Block& block) {

            VmaStatistics statistics;
            vmaGetVirtualBlockStatistics(block.block, &statistics);
            return size_t(statistics.allocationBytes);

        }

        bool GeometryPool::AllocateInBlock(Block& block, size_t count, size_t alignment, VmaVirtualAllocation& allocation,
            uint32_t& first) {

            VmaVirtualAllocationCreateInfo info = { .size = std::max(count, size_t(1)), .alignment = alignment };
            VkDeviceSize offset;
            if (vmaVirtualAllocate(block.block, &info, &allocation, &offset) != VK_SUCCESS)
                return false;

            first = uint32_t(offset);
            return true;

        }

        void GeometryPool::Free(Range* range) {

            vmaVirtualFree(range->vertexChunk->block, range->vertexAllocation);
            vmaVirtualFree(range->indexChunk->block, range->indexAllocation);
            delete range;

        }

        void GeometryPool::ReleasePendingFrees(bool all) {

            if (pendingFrees.empty())
                return;

            // The frames in flight might still draw the ranges
            auto frameIndex = Graphics::GraphicsDevice::DefaultDevice->GetFrameIndex();
            auto freedCount = std::erase_if(pendingFrees, [&](PendingFree& pending) {
                if (!all && frameIndex - pending.frameIndex < int32_t(Graphics::MemoryManager::framesToDeletion))
                    return false;
                Free(pending.range);
                return true;
                });
            if (!freedCount)
                return;

            // The buffers of empty chunks are released by the device once the frames in flight are done
            std::erase_if(vertexChunks, [](const Ref<VertexChunk>& chunk) { return vmaIsVirtualBlockEmpty(chunk->block); });
            std::erase_if(indexChunks, [](const Ref<IndexChunk>& chunk) { return vmaIsVirtualBlockEmpty(chunk->block); });

        }

    }

}
