#ifndef AE_GRAPHICSINITIALIZERS_H
#define AE_GRAPHICSINITIALIZERS_H

#include <volk.h>

namespace Atlas {

    namespace Graphics {

        namespace Initializers {

            VkCommandPoolCreateInfo InitCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

            VkCommandBufferAllocateInfo InitCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t bufferCount,
                VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        }

    }

}

#endif
