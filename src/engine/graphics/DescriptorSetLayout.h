#pragma once

#include "Common.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class ShaderVariant;
        class DescriptorPool;

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

        struct DescriptorSetSize {
            uint32_t dynamicUniformBufferCount = 0;
            uint32_t uniformBufferCount = 0;
            uint32_t dynamicStorageBufferCount = 0;
            uint32_t storageBufferCount = 0;
            uint32_t combinedImageSamplerCount = 0;
            uint32_t sampledImageCount = 0;
            uint32_t storageImageCount = 0;
            uint32_t samplerCount = 0;
        };

        class DescriptorSetLayout {

            friend ShaderVariant;
            friend DescriptorPool;

        public:
            DescriptorSetLayout(GraphicsDevice* device, const DescriptorSetLayoutDesc& desc);

            ~DescriptorSetLayout();

            bool IsCompatible(const Ref<DescriptorSetLayout>& that) const;

            VkDescriptorSetLayout layout = {};

            bool isComplete = false;
            bool bindless = false;

        private:
            GraphicsDevice* device;

            DescriptorSetSize size = {};

            std::vector<DescriptorSetBinding> bindings;

            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            std::vector<VkDescriptorBindingFlags> layoutBindingFlags;

        };

    }

}