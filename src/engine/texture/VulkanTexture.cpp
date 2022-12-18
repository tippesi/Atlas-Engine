#include "VulkanTexture.h"

#include "../EngineInstance.h"

namespace Atlas {

    namespace Texture {

        VulkanTexture::VulkanTexture(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
            VkSamplerAddressMode wrapping, VkFilter filtering, bool anisotropicFiltering, bool generateMipMaps)
            : width(width), height(height), depth(depth) {

            auto graphicsInstance = EngineInstance::GetGraphicsInstance();
            auto graphicsDevice = graphicsInstance->GetGraphicsDevice();

            VkImageUsageFlags additionalUsageFlags = {};
            if (generateMipMaps) {
                additionalUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }
            auto imageDesc = Graphics::ImageDesc {
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | additionalUsageFlags,
                .type = VK_IMAGE_TYPE_2D,
                .width = width,
                .height = height,
                .depth = depth,
                .format = format
            };
            image = graphicsDevice->CreateImage(imageDesc);

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = filtering,
                .mode = wrapping,
                .mipmapMode = generateMipMaps ? VK_SAMPLER_MIPMAP_MODE_LINEAR :
                              VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .maxLod = float(this->image->mipLevels),
                .anisotropicFiltering = generateMipMaps && anisotropicFiltering
            };
            sampler = graphicsDevice->CreateSampler(samplerDesc);

        }

        VulkanTexture::VulkanTexture(Common::Image<uint8_t>& image, bool anisotropicFiltering, bool generateMipMaps)
            : width(image.width), height(image.height) {

            auto graphicsInstance = EngineInstance::GetGraphicsInstance();
            auto graphicsDevice = graphicsInstance->GetGraphicsDevice();

            VkFormat format;
            switch(image.channels) {
                case 1: format = VK_FORMAT_R8_UNORM; break;
                case 2: format = VK_FORMAT_R8G8_UNORM; break;
                case 3: format = VK_FORMAT_R8G8B8_UNORM; break;
                default: format = VK_FORMAT_R8G8B8A8_UNORM; break;
            }

            VkImageUsageFlags additionalUsageFlags = {};
            if (generateMipMaps) {
                additionalUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }

            auto imageDesc = Graphics::ImageDesc {
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | additionalUsageFlags,
                .type = VK_IMAGE_TYPE_2D,
                .width = width,
                .height = height,
                .depth = depth,
                .format = format,
                .mipMapping = generateMipMaps
            };
            this->image = graphicsDevice->CreateImage(imageDesc);

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipmapMode = generateMipMaps ? VK_SAMPLER_MIPMAP_MODE_LINEAR :
                    VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .maxLod = float(this->image->mipLevels),
                .anisotropicFiltering = generateMipMaps && anisotropicFiltering
            };
            sampler = graphicsDevice->CreateSampler(samplerDesc);

            SetData(image.GetData());

        }

        void VulkanTexture::SetData(std::vector<uint8_t> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void VulkanTexture::SetData(std::vector<uint16_t> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

    }

}