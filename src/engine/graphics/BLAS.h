#pragma once

#include "Common.h"
#include "Buffer.h"

#include <vector>
#include <atomic>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;
        class ASBuilder;

        struct BLASDesc {
            VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

            bool isDynamic = false;
            // Allows to refit the BLAS after the vertices moved instead of building it again
            bool allowUpdate = false;

            std::vector<VkAccelerationStructureGeometryKHR> geometries;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
        };

        class BLAS {

            friend GraphicsDevice;
            friend ASBuilder;

        public:
            BLAS(GraphicsDevice* device, BLASDesc desc);

            ~BLAS();

            void Allocate();

            VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
            VkAccelerationStructureBuildSizesInfoKHR sizesInfo;
            VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;

            Ref<Buffer> buffer;
            VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

            VkDeviceAddress deviceAddress = 0;

            bool isDynamic = false;
            bool allowUpdate = false;
            std::atomic_bool isBuilt = false;

        private:
            GraphicsDevice* device;

            std::vector<VkAccelerationStructureGeometryKHR> geometries;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;

            VkBuildAccelerationStructureFlagsKHR flags;

        };

    }

}
