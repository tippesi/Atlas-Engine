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
            Image3D
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
            Image(GraphicsDevice* device, ImageDesc& desc);

            ~Image();

            void SetData(void* data, size_t offsetX, size_t offsetY, size_t offsetZ,
                size_t width, size_t height, size_t layers);

            VkImageType GetImageType() const;

            VkImage image;
            VmaAllocation allocation;

            VkImageView view;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageAspectFlags aspectFlags;
            VkAccessFlags accessMask = VK_ACCESS_MEMORY_READ_BIT |
                VK_ACCESS_MEMORY_WRITE_BIT;

            std::vector<VkImageView> layerViews;

            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
            uint32_t layers = 1;
            VkFormat format;

            uint32_t mipLevels = 1;

            ImageDomain domain;
            ImageType type;

        private:
            MemoryManager* memoryManager;

        };

    }

}

#endif