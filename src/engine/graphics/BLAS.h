#ifndef AE_GRAPHICSBLAS_H
#define AE_GRAPHICSBLAS_H

#include "Common.h"
#include "Buffer.h"

#include <vector>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct BLASDesc {
            Ref<Buffer> vertexBuffer;
            Ref<Buffer> indexBuffer;

            VkAccelerationStructureGeometryKHR geometry;
            VkAccelerationStructureBuildRangeInfoKHR buildRange;
        };

        class BLAS {

        public:
            BLAS(GraphicsDevice* device, BLASDesc desc);

            Ref<Buffer> buffer;
            VkAccelerationStructureKHR accelerationStructure;

        };

        class BLASBuilder {

        public:
            BLASBuilder() = default;

            void Build(std::vector<Ref<BLAS>> blases);

        private:

        };

    }

}

#endif