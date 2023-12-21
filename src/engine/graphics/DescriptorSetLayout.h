#pragma once

#include "Common.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;
        class ShaderVariant;

        struct DescriptorSetBinding {
            uint32_t bindingIdx = 0;
            VkDescriptorType descriptorType;

            uint32_t descriptorCount = 1;
            VkShaderStageFlags stageFlags = VK_SHADER_STAGE_ALL;

            uint64_t size = VK_WHOLE_SIZE;
            uint32_t arrayElement = 0;

            bool bindless = false;
        };

        struct DescriptorSetLayoutDesc {
            DescriptorSetBinding bindings[BINDINGS_PER_DESCRIPTOR_SET];
            uint32_t bindingCount = 0;
        };

        class DescriptorSetLayout {

            friend ShaderVariant;

        public:
            DescriptorSetLayout(GraphicsDevice* device, const DescriptorSetLayoutDesc& desc);

            ~DescriptorSetLayout();

            bool IsCompatible(const Ref<DescriptorSetLayout>& that) const;

            VkDescriptorSetLayout layout = {};

            bool isComplete = false;

        private:
            GraphicsDevice* device;

            std::vector<DescriptorSetBinding> bindings;

            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            std::vector<VkDescriptorBindingFlags> layoutBindingFlags;

        };

    }

}