#pragma once

#include "Common.h"

#include "BLAS.h"
#include "TLAS.h"
#include "QueryPool.h"
#include "CommandList.h"

#include <span>

namespace Atlas {

    namespace Graphics {

        struct ASGeometryRegion {
            size_t indexCount = 0;
            size_t indexOffset;

            bool opaque = true;
        };

        class ASBuilder {

        public:
            ASBuilder() = default;

            BLASDesc GetBLASDescForTriangleGeometry(Ref<Buffer> vertexBuffer, Ref<Buffer> indexBuffer,
                size_t vertexCount, size_t vertexSize, size_t indexSize, std::span<ASGeometryRegion> regions);

            int32_t BuildBLAS(std::span<Ref<BLAS>> blases, CommandList* commandList = nullptr);

            /**
             * Refits built BLASes which allow updates to the current state of their vertices
             * @note The topology has to stay the same, only the vertex positions can change
             */
            void UpdateBLAS(std::span<Ref<BLAS>> blases, CommandList* commandList);
            
            Buffer* BuildTLAS(Ref<TLAS>& tlas, std::span<VkAccelerationStructureInstanceKHR> instances, CommandList* commandList = nullptr);

        private:
            void BuildBLASBatch(const std::span<uint32_t>& batchIndices, std::span<Ref<BLAS>>& blases, 
                Ref<Buffer>& scratchBuffer, Ref<QueryPool>& queryPool, CommandList* commandList);

            void CompactBLASBatch(const std::span<uint32_t>& batchIndices,
                std::span<Ref<BLAS>>& blases, Ref<QueryPool>& queryPool, CommandList* commandList);

            Ref<Graphics::Buffer> scratchBuffer = nullptr;
            Ref<Graphics::MultiBuffer> instanceBuffer = nullptr;

        };

    }

}