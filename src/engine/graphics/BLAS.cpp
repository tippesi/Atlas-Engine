#include "BLAS.h"

#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        BLAS::BLAS(GraphicsDevice* device, BLASDesc desc) : device(device), geometries(desc.geometries),
            buildRanges(desc.buildRanges) {

            buildGeometryInfo = {};
            buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildGeometryInfo.flags = desc.flags;
            buildGeometryInfo.geometryCount = uint32_t(geometries.size());
            buildGeometryInfo.pGeometries = geometries.data();

            sizesInfo = {};
            sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            std::vector<uint32_t> maxPrimitivesCount;
            for (auto& range : buildRanges) {
                maxPrimitivesCount.push_back(range.primitiveCount);
            }

            vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildGeometryInfo, maxPrimitivesCount.data(), &sizesInfo);



        }

        void BLAS::Allocate(size_t size) {

            VkAccelerationStructureCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.size = size;



        }

    }

}