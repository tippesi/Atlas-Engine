#pragma once

#include "../System.h"
#include "../graphics/Buffer.h"

#include <mutex>
#include <vector>

namespace Atlas {

    namespace Buffer {

        /**
         * Vertices and indices of the meshes in shared buffers. Meshes with the same vertex formats share the vertex
         * buffers of a chunk, meshes with the same index type the index buffer of an index chunk. A draw selects its
         * mesh with the first index and the vertex offset. Chunks grow up to a maximum size and are released once
         * they are empty.
         */
        class GeometryPool {

        public:
            // Suballocated in elements
            struct Block {
                VmaVirtualBlock block = VK_NULL_HANDLE;
                size_t capacity = 0;

                Block() = default;
                Block(const Block&) = delete;
                Block& operator=(const Block&) = delete;
                ~Block();
            };

            struct VertexChunk : Block {
                std::vector<VkFormat> formats;
                // One buffer per vertex format, in the order of the formats
                std::vector<Ref<Graphics::Buffer>> buffers;
            };

            struct IndexChunk : Block {
                VkIndexType type;
                Ref<Graphics::Buffer> buffer;
            };

            // The geometry of a mesh
            struct Range {
                Ref<VertexChunk> vertexChunk;
                Ref<IndexChunk> indexChunk;
                VmaVirtualAllocation vertexAllocation = VK_NULL_HANDLE;
                VmaVirtualAllocation indexAllocation = VK_NULL_HANDLE;
                uint32_t firstVertex = 0;
                uint32_t firstIndex = 0;
            };

            // Like the objects of the graphics device, the range is reused some frames after its last reference is gone
            static Ref<Range> Allocate(const std::vector<VkFormat>& vertexFormats, VkIndexType indexType,
                size_t vertexCount, size_t indexCount);

            // Uploads with the transfer queue, the graphics queue draws from other parts of the chunks meanwhile
            static void Upload(const Range& range, const std::vector<std::pair<const void*, size_t>>& vertexData,
                const void* indexData, size_t indexCount);

            // Frees the ranges whose frames are done and releases empty chunks
            static void Update();

            // Frees all released ranges, ranges released afterwards are freed right away
            static void Shutdown();

        private:
            struct PendingFree {
                Range* range;
                int32_t frameIndex;
            };

            static void Release(Range* range);

            // Chunks of a layout double in capacity up to a maximum, the request might need more
            static size_t GetChunkCapacity(size_t previousCapacity, size_t minCapacity, size_t maxCapacity, size_t count);

            static size_t GetUsedCount(const Block& block);

            static bool AllocateInBlock(Block& block, size_t count, size_t alignment, VmaVirtualAllocation& allocation,
                uint32_t& first);

            static void Free(Range* range);

            static void ReleasePendingFrees(bool all);

            static std::mutex mutex;
            static std::vector<Ref<VertexChunk>> vertexChunks;
            static std::vector<Ref<IndexChunk>> indexChunks;
            static std::vector<PendingFree> pendingFrees;
            static bool shutdown;

        };

    }

}
