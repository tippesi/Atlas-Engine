#pragma once

#include "Common.h"

#include "BLAS.h"
#include "TLAS.h"
#include "QueryPool.h"
#include "CommandList.h"

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
                size_t vertexCount, size_t vertexSize, size_t indexSize, std::vector<ASGeometryRegion> regions);

            int32_t BuildBLAS(std::vector<Ref<BLAS>>& blases, CommandList* commandList = nullptr);
            
            Ref<Buffer> BuildTLAS(Ref<TLAS>& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances, CommandList* commandList = nullptr);

        private:
            void BuildBLASBatch(const std::vector<uint32_t>& batchIndices, std::vector<Ref<BLAS>>& blases, 
                Ref<Buffer>& scratchBuffer, Ref<QueryPool>& queryPool, CommandList* commandList);

            void CompactBLASBatch(const std::vector<uint32_t>& batchIndices,
                std::vector<Ref<BLAS>>& blases, Ref<QueryPool>& queryPool, CommandList* commandList);

            Ref<Graphics::Buffer> scratchBuffer = nullptr;

        };

    }

}