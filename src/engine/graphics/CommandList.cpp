#include "CommandList.h"

namespace Atlas {

    namespace Graphics {

        CommandList::CommandList(VkDevice device, uint32_t queueFamilyIndex) : device(device) {

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(queueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer))

            isComplete = true;

        }

        CommandList::~CommandList() {

            vkDestroyCommandPool(device, commandPool, nullptr);

        }

    }

}