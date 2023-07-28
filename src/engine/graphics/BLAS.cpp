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

            rangeInfo = buildRanges.data();

            std::vector<uint32_t> maxPrimitivesCount;
            for (auto& range : buildRanges) {
                maxPrimitivesCount.push_back(range.primitiveCount);
            }

            vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildGeometryInfo, maxPrimitivesCount.data(), &sizesInfo);

            Allocate();

        }

        BLAS::~BLAS() {

            vkDestroyAccelerationStructureKHR(device->device, accelerationStructure, nullptr);

        }

        void BLAS::Allocate() {

            BufferDesc desc = {
                .usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                              | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .domain = BufferDomain::Device,
                .size = sizesInfo.accelerationStructureSize
            };
            buffer = device->CreateBuffer(desc);

            VkAccelerationStructureCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            createInfo.size = sizesInfo.accelerationStructureSize;
            createInfo.buffer = buffer->buffer;

            VK_CHECK(vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &accelerationStructure))

        }

        VkDeviceAddress BLAS::GetDeviceAddress() {

            return buffer->GetDeviceAddress();

        }

    }

}