#include "DescriptorSetLayout.h"

#include "GraphicsDevice.h"

namespace Atlas {

	namespace Graphics {

		DescriptorSetLayout::DescriptorSetLayout(GraphicsDevice* device, const DescriptorSetLayoutDesc& desc) :
            device(device) {

            // We need this flag to support arrays of descriptors even in non-bindless mode (e.g. array of textures, where only one index is bound)
            VkDescriptorBindingFlags defaultBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

            bindings.resize(desc.bindingCount);
            layoutBindings.resize(desc.bindingCount);
            layoutBindingFlags.resize(desc.bindingCount, defaultBindingFlags);

            bool bindlessAllowed = true;
            bool bindlessNeeded = false;

            for (uint32_t i = 0; i < desc.bindingCount; i++) {
                VkDescriptorSetLayoutBinding layoutBinding = {};
                layoutBinding.binding = desc.bindings[i].bindingIdx;
                layoutBinding.descriptorCount = desc.bindings[i].descriptorCount;
                layoutBinding.descriptorType = desc.bindings[i].descriptorType;
                layoutBinding.stageFlags = desc.bindings[i].stageFlags;

                switch (layoutBinding.descriptorType) {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: 
                    size.dynamicUniformBufferCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: 
                    size.uniformBufferCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: 
                    size.dynamicStorageBufferCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: 
                    size.storageBufferCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: 
                    size.combinedImageSamplerCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: 
                    size.sampledImageCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: 
                    size.storageImageCount += layoutBinding.descriptorCount; break;
                case VK_DESCRIPTOR_TYPE_SAMPLER: 
                    size.samplerCount += layoutBinding.descriptorCount; break;
                default: break;
                }

                bindlessAllowed &= (layoutBinding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) &&
                    (layoutBinding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
                bindlessNeeded |= desc.bindings[i].bindless;

                bindings[i] = desc.bindings[i];
                layoutBindings[i] = layoutBinding;
            }

            VkDescriptorSetLayoutCreateInfo setInfo = {};
            setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            setInfo.pNext = nullptr;
            setInfo.bindingCount = desc.bindingCount;
            setInfo.flags = bindlessAllowed && bindlessNeeded ?
                VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
            setInfo.pBindings = desc.bindingCount ? layoutBindings.data() : VK_NULL_HANDLE;

            VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo = {};
            if (bindlessAllowed && bindlessNeeded) {
                bindless = true;

                for (uint32_t i = 0; i < desc.bindingCount; i++) {
                    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

                    if (desc.bindings[i].bindless) {
                        //bindingFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    }

                    layoutBindingFlags[i] = bindingFlags;
                }
            }

	    extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            extendedInfo.bindingCount = desc.bindingCount;
            extendedInfo.pBindingFlags = desc.bindingCount ? layoutBindingFlags.data() : VK_NULL_HANDLE;

            setInfo.pNext = &extendedInfo;

            VK_CHECK(vkCreateDescriptorSetLayout(device->device, &setInfo, nullptr, &layout))

            isComplete = true;
            
        }

		DescriptorSetLayout::~DescriptorSetLayout() {

            vkDestroyDescriptorSetLayout(device->device, layout, nullptr);

		}

        bool DescriptorSetLayout::IsCompatible(const Ref<DescriptorSetLayout>& that) const {

            if (that->layoutBindings.size() > layoutBindings.size())
                return false;

            for (size_t i = 0; i < that->layoutBindings.size(); i++) {

                bool found = false;
                const auto& otherBinding = that->bindings[i];

                for (size_t j = 0; j < layoutBindings.size(); j++) {
                    const auto& binding = bindings[j];
                    // Only check identical bindings
                    if (binding.bindingIdx != otherBinding.bindingIdx)
                        continue;

                    if ((binding.descriptorCount != otherBinding.descriptorCount && !binding.bindless) ||
                        binding.stageFlags != otherBinding.stageFlags)
                        return false;

                    // All shaders automatically use dynamic uniform buffers, so 
                    // potentially revert this change here
                    auto otherDescriptorType = otherBinding.descriptorType;
                    if (otherDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                        otherDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                    auto thisDescriptorType = binding.descriptorType;
                    if (thisDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                        thisDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                    if (otherDescriptorType != thisDescriptorType)
                        return false;

                    found = true;
                }

                if (!found)
                    return false;

            }

            return true;

        }

	}

}
