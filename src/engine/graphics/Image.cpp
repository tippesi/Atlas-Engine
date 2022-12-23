#include "Image.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        Image::Image(GraphicsDevice *device, ImageDesc &desc) : aspectFlags(desc.aspectFlags),
            width(desc.width), height(desc.height), depth(desc.depth), layers(desc.layers), format(desc.format),
            domain(desc.domain), type(desc.type), memoryManager(device->memoryManager) {

            VkExtent3D imageExtent;
            imageExtent.width = desc.width;
            imageExtent.height = desc.height;
            imageExtent.depth = desc.depth;

            VkImageCreateInfo imageInfo = Initializers::InitImageCreateInfo(desc.format,
                desc.usageFlags, imageExtent, GetImageType());
            if (desc.mipMapping) {
                mipLevels = uint32_t(floor(log2(glm::max(float(width), float(height)))) + 1);
                imageInfo.mipLevels = mipLevels;
            }
            if (desc.type == ImageType::Image1DArray || desc.type == ImageType::Image2DArray) {
                imageInfo.arrayLayers = desc.layers;
            }

            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = desc.domain == ImageDomain::Host ?
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            VK_CHECK(vmaCreateImage(memoryManager->allocator, &imageInfo,
                &allocationCreateInfo, &image, &allocation, nullptr))

            VkImageViewType viewType;
            switch(desc.type) {
                case ImageType::Image1D: viewType = VK_IMAGE_VIEW_TYPE_1D; break;
                case ImageType::Image1DArray: viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY; break;
                case ImageType::Image2D: viewType = VK_IMAGE_VIEW_TYPE_2D; break;
                case ImageType::Image2DArray: viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
                case ImageType::Image3D: viewType = VK_IMAGE_VIEW_TYPE_3D; break;
                default: viewType = VK_IMAGE_VIEW_TYPE_3D; break;
            }


            VkImageViewCreateInfo imageViewInfo = Initializers::InitImageViewCreateInfo(desc.format,
                image, desc.aspectFlags, viewType, imageInfo.arrayLayers);
            if (desc.mipMapping) {
                imageViewInfo.subresourceRange.levelCount = mipLevels;
            }
            VK_CHECK(vkCreateImageView(memoryManager->device, &imageViewInfo, nullptr, &view))

            // This will just duplicate the view for single-layered images, don't care for now
            layerViews.resize(layers);
            for (uint32_t i = 0; i < layers; i++) {
                imageViewInfo.subresourceRange.baseArrayLayer = i;
                imageViewInfo.subresourceRange.layerCount = 1;
                VK_CHECK(vkCreateImageView(memoryManager->device, &imageViewInfo, nullptr, &layerViews[i]))
            }

            if (desc.data) SetData(desc.data, 0, 0, 0, desc.width, desc.height, desc.layers);

        }

        Image::~Image() {

            vkDestroyImageView(memoryManager->device, view, nullptr);
            for (auto layerView : layerViews) {
                vkDestroyImageView(memoryManager->device, layerView, nullptr);
            }
            vmaDestroyImage(memoryManager->allocator, image, allocation);

        }

        void Image::SetData(void *data, size_t offsetX, size_t offsetY, size_t offsetZ,
            size_t width, size_t height, size_t depth) {

            if (domain == ImageDomain::Device) {
                VkOffset3D offset = {};
                offset.x = int32_t(offsetX);
                offset.y = int32_t(offsetY);
                offset.z = int32_t(offsetZ);

                VkExtent3D extent = {};
                extent.width = uint32_t(width);
                extent.height = uint32_t(height);
                extent.depth = uint32_t(depth);

                memoryManager->transferManager->UploadImageData(data, this, offset, extent);
            }

        }

        VkImageType Image::GetImageType() const {

            switch(type) {
                case ImageType::Image1D:
                case ImageType::Image1DArray: return VK_IMAGE_TYPE_1D;
                case ImageType::Image2D:
                case ImageType::Image2DArray: return VK_IMAGE_TYPE_2D;
                case ImageType::Image3D: return VK_IMAGE_TYPE_3D;
            }

        }

    }

}