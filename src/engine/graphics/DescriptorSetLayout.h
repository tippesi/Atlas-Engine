#pragma once

#include "Common.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        struct DescriptorSetBinding {
            uint32_t bindingIdx = 0;
            VkDescriptorType descriptorType;

            uint32_t descriptorCount = 1;
            VkShaderStageFlags stageFlags = VK_SHADER_STAGE_ALL;

            bool bindless = false;
        };

        struct DescriptorSetLayoutDesc {
            DescriptorSetBinding bindings[BINDINGS_PER_DESCRIPTOR_SET];
            uint32_t bindingCount = 0;
        };

        class DescriptorSetLayout {

        public:
            DescriptorSetLayout(GraphicsDevice* device, const DescriptorSetLayoutDesc& desc);

            ~DescriptorSetLayout();

            VkDescriptorSetLayout layout = {};

            bool isComplete = false;

        private:
            GraphicsDevice* device;

        };

    }

}