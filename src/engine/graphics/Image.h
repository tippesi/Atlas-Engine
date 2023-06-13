#ifndef AE_GRAPHICSTEXTURE_H
#define AE_GRAPHICSTEXTURE_H

#include "Common.h"

#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class MemoryManager;

        enum class ImageDomain {
            Device = 0,
            Host = 1
        };

        enum class ImageType {
            Image1D = 0,
            Image1DArray,
            Image2D,
            Image2DArray,
            Image3D,
            ImageCube
        };

        struct ImageDesc {
            VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ImageDomain domain = ImageDomain::Device;
            ImageType type = ImageType::Image2D;
            VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
            uint32_t layers = 1;
            VkFormat format;

            bool mipMapping = false;

            void* data = nullptr;
        };

        struct ImageAllocation {
            VkImage image;
            VmaAllocation allocation;
        };

        class Image {

        public:
            Image(GraphicsDevice* device, const ImageDesc& desc);

            ~Image();

            void SetData(void* data, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                uint32_t width, uint32_t height, uint32_t depth, uint32_t layerOffset = 0,
                uint32_t layerCount = 1);

            VkImageType GetImageType() const;

            VkImage image;
            VmaAllocation allocation;

            VkImageView view;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageAspectFlags aspectFlags;
            VkAccessFlags accessMask = VK_ACCESS_NONE;

            std::vector<VkImageView> attachmentViews;

            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
            uint32_t layers = 1;
            VkFormat format;

            uint32_t mipLevels = 1;

            ImageDomain domain;
            ImageType type;

        private:
            GraphicsDevice* device;
            MemoryManager* memoryManager;

        };

    }

}

#endif