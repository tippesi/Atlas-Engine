#pragma once

#include "Common.h"
#include "Buffer.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct TLASDesc {
            VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        };

        class TLAS {

        public:
            TLAS(GraphicsDevice* device, TLASDesc desc);

            ~TLAS();

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