#include "Image.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        Image::Image(GraphicsDevice *device, ImageDesc &desc) : type(desc.type), width(desc.width), height(desc.height),
            depth(desc.depth), format(desc.format), domain(desc.domain), memoryManager(device->memoryManager) {

            VkExtent3D imageExtent;
            imageExtent.width = desc.width;
            imageExtent.height = desc.height;
            imageExtent.depth = desc.depth;

            VkImageCreateInfo imageInfo = Initializers::InitImageCreateInfo(desc.format,
                desc.usageFlags, imageExtent, desc.type);
            if (desc.mipMapping) {
                mipLevels = uint32_t(floor(log2(glm::max(float(width), float(height)))) + 1);
                imageInfo.mipLevels = mipLevels;
            }

            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = desc.domain == ImageDomain::Host ?
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            VK_CHECK(vmaCreateImage(memoryManager->allocator, &imageInfo,
                &allocationCreateInfo, &image, &allocation, nullptr))

            VkImageViewType viewType;
            switch(desc.type) {
                case VK_IMAGE_TYPE_1D: viewType = VK_IMAGE_VIEW_TYPE_1D; break;
                case VK_IMAGE_TYPE_2D: viewType = VK_IMAGE_VIEW_TYPE_2D; break;
                case VK_IMAGE_TYPE_3D: viewType = VK_IMAGE_VIEW_TYPE_3D; break;
                default: viewType = VK_IMAGE_VIEW_TYPE_3D; break;
            }

            VkImageViewCreateInfo imageViewInfo = Initializers::InitImageViewCreateInfo(desc.format,
                image, desc.aspectFlags, viewType, desc.depth);
            if (desc.mipMapping) {
                imageViewInfo.subresourceRange.levelCount = mipLevels;
            }

            VK_CHECK(vkCreateImageView(memoryManager->device, &imageViewInfo, nullptr, &view))

            if (desc.data) SetData(desc.data, 0, 0, 0, desc.width, desc.height, desc.depth);

        }

        Image::~Image() {

            vkDestroyImageView(memoryManager->device, view, nullptr);
            memoryManager->DestroyAllocation(ImageAllocation { image, allocation });

        }

        void Image::SetData(void *data, size_t offsetX, size_t offsetY, size_t offsetZ,
            size_t width, size_t height, size_t depth) {

            if (domain == ImageDomain::Device) {
                VkOffset3D offset = {};
                offset.x = offsetX;
                offset.y = offsetY;
                offset.z = offsetZ;

                VkExtent3D extent = {};
                extent.width = width;
                extent.height = height;
                extent.depth = depth;

                memoryManager->transferManager->UploadImageData(data, this, offset, extent);
            }

        }

    }

}