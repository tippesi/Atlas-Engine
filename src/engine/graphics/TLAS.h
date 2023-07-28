#ifndef AE_GRAPHICSTLAS_H
#define AE_GRAPHICSTLAS_H

#include "Common.h"
#include "Buffer.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct TLASDesc {
            VkBuildAccelerationStructureFlagsKHR flags;

            VkAccelerationStructureGeometryKHR geometry;
            VkAccelerationStructureBuildRangeInfoKHR buildRange;
        };

        class TLAS {

        public:
            TLAS(GraphicsDevice* device, TLASDesc desc);

            void Allocate(VkDeviceAddress instancesAddress, uint32_t instancesCount, bool update);

            VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
            VkAccelerationStructureBuildSizesInfoKHR sizesInfo;
            VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;

            Ref<Buffer> buffer;
            VkAccelerationStructureKHR accelerationStructure;

        private:
            GraphicsDevice* device;

            VkAccelerationStructureGeometryKHR geometry;
            VkAccelerationStructureBuildRangeInfoKHR buildRange;

        };

    }

}

#endif