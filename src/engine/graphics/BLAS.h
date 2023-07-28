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
            VkBuildAccelerationStructureFlagsKHR flags;

            std::vector<VkAccelerationStructureGeometryKHR> geometries;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
        };

        class BLAS {

            friend GraphicsDevice;

        public:
            BLAS(GraphicsDevice* device, BLASDesc desc);

            void Allocate();

            VkDeviceAddress GetDeviceAddress();

            VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
            VkAccelerationStructureBuildSizesInfoKHR sizesInfo;
            VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;

            Ref<Buffer> buffer;
            VkAccelerationStructureKHR accelerationStructure;

        private:
            GraphicsDevice* device;

            std::vector<VkAccelerationStructureGeometryKHR> geometries;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;

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