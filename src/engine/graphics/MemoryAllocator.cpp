#include "MemoryAllocator.h"

namespace Atlas {

    namespace Graphics {

        MemoryAllocator::MemoryAllocator(const Atlas::Graphics::MemoryAllocatorDesc desc, bool &success) {

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = desc.physicalDevice;
            allocatorInfo.device = desc.device;
            allocatorInfo.instance = desc.instance;
            VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator))

        }

    }

}