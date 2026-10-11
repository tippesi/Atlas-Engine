#include "BLAS.h"

#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        BLAS::BLAS(GraphicsDevice* device, BLASDesc desc) : isDynamic(desc.isDynamic), device(device), 
            geometries(desc.geometries), buildRanges(desc.buildRanges), flags(desc.flags)    {

            buildGeometryInfo = {};
            buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildGeometryInfo.flags = desc.flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
            buildGeometryInfo.geometryCount = uint32_t(geometries.size());
            buildGeometryInfo.pGeometries = geometries.data();

            sizesInfo = {};
            sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            rangeInfo = buildRanges.data();

            std::vector<uint32_t> maxPrimitivesCount;
            for (auto& range : buildRanges) {
                maxPrimitivesCount.push_back(range.primitiveCount);
            }

            vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildGeometryInfo, maxPrimitivesCount.data(), &sizesInfo);

        }

        BLAS::~BLAS() {

            vkDestroyAccelerationStructureKHR(device->device, accelerationStructure, nullptr);

        }

        void BLAS::Allocate() {

            BufferDesc desc = {
                .usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                              | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .domain = BufferDomain::Device,
                .size = sizesInfo.accelerationStructureSize,
                .priority = 1.0f
            };

            buffer = device->CreateBuffer(desc);

            VkAccelerationStructureCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            createInfo.size = sizesInfo.accelerationStructureSize;
            createInfo.buffer = buffer->buffer;

            VK_CHECK(vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &accelerationStructure))

            // TLAS instances must reference the acceleration structure address,
            // which is not necessarily the address of its backing buffer.
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = accelerationStructure;
            deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device->device, &addressInfo);

        }

    }

}
