#ifndef GRAPHICSASBUILDER_H
#define GRAPHICSASBUILDER_H

#include "Common.h"

#include "BLAS.h"
#include "TLAS.h"

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

            void BuildBLAS(std::vector<Ref<BLAS>>& blases);
            
            Ref<Buffer> BuildTLAS(Ref<TLAS>& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances);

        private:
            void BuildBLASBatch(const std::vector<uint32_t>& batchIndices,
                std::vector<Ref<BLAS>>& blases, Ref<Buffer>& scratchBuffer);

        };

    }

}

#endif