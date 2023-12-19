#include "DescriptorSetLayout.h"

#include "GraphicsDevice.h"

namespace Atlas {

	namespace Graphics {

		DescriptorSetLayout::DescriptorSetLayout(GraphicsDevice* device, const DescriptorSetLayoutDesc& desc) :
            device(device) {

            std::vector<VkDescriptorSetLayoutBinding> layoutBindings(desc.bindingCount);
            std::vector<VkDescriptorBindingFlags> layoutBindingFlags(desc.bindingCount);

            bool bindlessAllowed = true;
            bool bindlessNeeded = false;

            for (uint32_t i = 0; i < desc.bindingCount; i++) {
                VkDescriptorSetLayoutBinding layoutBinding = {};
                layoutBinding.binding = desc.bindings[i].bindingIdx;
                layoutBinding.descriptorCount = desc.bindings[i].descriptorCount;
                layoutBinding.descriptorType = desc.bindings[i].descriptorType;
                layoutBinding.stageFlags = desc.bindings[i].stageFlags;

                bindlessAllowed &= (layoutBinding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) &&
                    (layoutBinding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
                bindlessNeeded |= desc.bindings[i].bindless;

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
                for (uint32_t i = 0; i < desc.bindingCount; i++) {
                    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

                    if (desc.bindings[i].bindless) {
                        bindingFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    }

                    layoutBindingFlags[i] = bindingFlags;
                }

                extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                extendedInfo.bindingCount = desc.bindingCount;
                extendedInfo.pBindingFlags = desc.bindingCount ? layoutBindingFlags.data() : VK_NULL_HANDLE;

                setInfo.pNext = &extendedInfo;
            }

            VK_CHECK(vkCreateDescriptorSetLayout(device->device, &setInfo, nullptr, &layout))

            isComplete = true;
            
        }

		DescriptorSetLayout::~DescriptorSetLayout() {

            vkDestroyDescriptorSetLayout(device->device, layout, nullptr);

		}

	}

}