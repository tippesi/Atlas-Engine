#ifndef AE_GRAPHICSTEXTURE_H
#define AE_GRAPHICSTEXTURE_H

#include "Common.h"
#include "MemoryManager.h"

namespace Atlas {

    namespace Graphics {

        class Device;

        enum class ImageDomain {
            Device = 0,
            Host = 1
        };

        struct ImageDesc {
            VkImageUsageFlags usageFlags;
            ImageDomain domain = ImageDomain::Device;
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
            VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
            VkFormat format;

            void* data = nullptr;
        };

        class Image {

        public:
            Image(GraphicsDevice* device, ImageDesc& desc);

            ~Image();

            void SetData(void* data, size_t offsetX, size_t offsetY, size_t offsetZ,
                size_t width, size_t height, size_t depth);

            VkImage image;
            VmaAllocation allocation;

            VkImageView view;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
            VkFormat format;

            ImageDomain domain;

        private:
            MemoryManager* memoryManager;

        };

    }

}

#endif