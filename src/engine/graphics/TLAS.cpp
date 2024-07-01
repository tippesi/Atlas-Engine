#include "TLAS.h"

#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        TLAS::TLAS(GraphicsDevice* device, TLASDesc desc) : device(device) {

            geometry = {};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

            buildGeometryInfo = {};
            buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildGeometryInfo.flags = desc.flags;
            buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;

            buildRange = {};

            rangeInfo = &buildRange;

        }

        TLAS::~TLAS() {

            if (isComplete)
                vkDestroyAccelerationStructureKHR(device->device, accelerationStructure, nullptr);

        }

        void TLAS::Allocate(VkDeviceAddress instancesAddress, uint32_t instancesCount, bool update) {

            VkAccelerationStructureGeometryInstancesDataKHR instances = {};
            instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            instances.data.deviceAddress = instancesAddress;

            geometry.geometry.instances = instances;

            buildGeometryInfo.geometryCount = 1;
            buildGeometryInfo.pGeometries = &geometry;
            buildGeometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            sizesInfo = {};
            sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildGeometryInfo, &instancesCount, &sizesInfo);

            buildRange.primitiveCount = instancesCount;

            // Don't want to reallocate on update. Keep in mind that the new instanceCount has
            // to be lower or equal than when this TLAS was last fully allocated
            if (update)
                return;

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
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            createInfo.size = sizesInfo.accelerationStructureSize;
            createInfo.buffer = buffer->buffer;

            VK_CHECK(vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &accelerationStructure));

            isComplete = true;

        }

    }

}