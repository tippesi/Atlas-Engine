#include "Initializers.h"

namespace Atlas {

    namespace Graphics {

        namespace Initializers {

            VkCommandPoolCreateInfo InitCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {

                VkCommandPoolCreateInfo commandPoolInfo = {};
                commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                commandPoolInfo.pNext = nullptr;
                commandPoolInfo.queueFamilyIndex = queueFamilyIndex;
                commandPoolInfo.flags = flags;

                return commandPoolInfo;

            }

            VkCommandBufferAllocateInfo InitCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t bufferCount,
                VkCommandBufferLevel level) {

                VkCommandBufferAllocateInfo cmdAllocInfo = {};
                cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cmdAllocInfo.pNext = nullptr;
                cmdAllocInfo.commandPool = commandPool;
                cmdAllocInfo.commandBufferCount = bufferCount;
                cmdAllocInfo.level = level;

                return cmdAllocInfo;

            }

        }

    }

}