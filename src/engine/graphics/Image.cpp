#include "Image.h"
#include "Format.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        Image::Image(GraphicsDevice *device, const ImageDesc &desc) : aspectFlags(desc.aspectFlags),
            width(desc.width), height(desc.height), depth(desc.depth), layers(desc.layers), format(desc.format),
            domain(desc.domain), type(desc.type), device(device), memoryManager(device->memoryManager) {

            VkExtent3D imageExtent;
            imageExtent.width = desc.width;
            imageExtent.height = desc.height;
            imageExtent.depth = desc.depth;

            bitDepth = GetFormatSize(desc.format) / GetFormatChannels(desc.format);

            VkImageCreateInfo imageInfo = Initializers::InitImageCreateInfo(desc.format,
                desc.usageFlags, imageExtent, GetImageType());
            if (desc.mipMapping && !desc.mipLevels) {
                mipLevels = uint32_t(floor(log2(glm::max(float(width), float(height)))) + 1);
                imageInfo.mipLevels = mipLevels;
            }
            if (desc.mipMapping && desc.mipLevels) {
                imageInfo.mipLevels = desc.mipLevels;
            }
            if (desc.type == ImageType::Image1DArray || desc.type == ImageType::Image2DArray ||
                desc.type == ImageType::ImageCube) {
                imageInfo.arrayLayers = desc.layers;
            }
            if (desc.type == ImageType::ImageCube) {
                imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            }

            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = 
                VMA_MEMORY_USAGE_AUTO;
            // Seems to be faster to just use that pool everywhere for all images. 
            // My guess is it is actually faster since the memory doesn't change constantly (not many new allocations)
            
            if (desc.dedicatedMemoryPool != nullptr)
                allocationCreateInfo.pool = *desc.dedicatedMemoryPool;

            VK_CHECK(vmaCreateImage(memoryManager->allocator, &imageInfo,
                &allocationCreateInfo, &image, &allocation, nullptr))

            VkImageViewType viewType;
            switch(desc.type) {
                case ImageType::Image1D: viewType = VK_IMAGE_VIEW_TYPE_1D; break;
                case ImageType::Image1DArray: viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY; break;
                case ImageType::Image2D: viewType = VK_IMAGE_VIEW_TYPE_2D; break;
                case ImageType::Image2DArray: viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
                case ImageType::Image3D: viewType = VK_IMAGE_VIEW_TYPE_3D; break;
                case ImageType::ImageCube: viewType = VK_IMAGE_VIEW_TYPE_CUBE; break;
                default: viewType = VK_IMAGE_VIEW_TYPE_3D; break;
            }

            VkImageViewCreateInfo imageViewInfo = Initializers::InitImageViewCreateInfo(desc.format,
                image, desc.aspectFlags, viewType, imageInfo.arrayLayers);
            if (desc.mipMapping) {
                mipMapViews.resize(mipLevels);
                for (uint32_t i = 0; i < mipLevels; i++) {
                    imageViewInfo.subresourceRange.baseMipLevel = i;
                    imageViewInfo.subresourceRange.levelCount = 1;
                    VK_CHECK(vkCreateImageView(device->device, &imageViewInfo, nullptr, &mipMapViews[i]))
                }
                imageViewInfo.subresourceRange.baseMipLevel = 0;
                imageViewInfo.subresourceRange.levelCount = mipLevels;
            }
            VK_CHECK(vkCreateImageView(device->device, &imageViewInfo, nullptr, &view))
            
            if (desc.type == ImageType::ImageCube) {
                // We need to set it back to a 2D view type when rendering to each cubemap face
                imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            }

            attachmentViews.resize(layers);
            for (uint32_t i = 0; i < layers; i++) {
                imageViewInfo.subresourceRange.baseArrayLayer = i;
                imageViewInfo.subresourceRange.layerCount = 1;
                // For attachments, we only want one mip level
                imageViewInfo.subresourceRange.levelCount = 1;
                VK_CHECK(vkCreateImageView(device->device, &imageViewInfo, nullptr, &attachmentViews[i]))
            }

            if (desc.data) SetData(desc.data, 0, 0, 0, desc.width, desc.height, desc.depth, 0, desc.layers);

        }

        Image::~Image() {

            vkDestroyImageView(device->device, view, nullptr);
            for (auto layerView : attachmentViews) {
                vkDestroyImageView(device->device, layerView, nullptr);
            }
            for (auto layerView : mipMapViews) {
                vkDestroyImageView(device->device, layerView, nullptr);
            }
            vmaDestroyImage(memoryManager->allocator, image, allocation);

        }

        void Image::SetData(void *data, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
            uint32_t width, uint32_t height, uint32_t depth, uint32_t layerOffset, uint32_t layerCount) {

            if (domain == ImageDomain::Device) {
                VkOffset3D offset = {};
                offset.x = int32_t(offsetX);
                offset.y = int32_t(offsetY);
                offset.z = int32_t(offsetZ);

                VkExtent3D extent = {};
                extent.width = uint32_t(width);
                extent.height = uint32_t(height);
                extent.depth = uint32_t(depth);

                memoryManager->transferManager->UploadImageData(data, this, offset, extent, layerOffset, layerCount);
            }

        }

        void Image::GenerateMipMaps() {

            if (domain == ImageDomain::Device) {
                memoryManager->transferManager->GenerateMipMaps(this);
            }

        }

        VkImageType Image::GetImageType() const {

            switch(type) {
                case ImageType::Image1D:
                case ImageType::Image1DArray: return VK_IMAGE_TYPE_1D;
                case ImageType::Image2D:
                case ImageType::ImageCube:
                case ImageType::Image2DArray: return VK_IMAGE_TYPE_2D;
                case ImageType::Image3D: return VK_IMAGE_TYPE_3D;
                default: return VK_IMAGE_TYPE_2D;
            }

        }

    }

}
