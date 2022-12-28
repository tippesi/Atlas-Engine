#include "VulkanTexture.h"

#include "../graphics/Instance.h"
#include "../graphics/Format.h"

namespace Atlas {

    namespace Texture {

        VulkanTexture::VulkanTexture(int32_t width, int32_t height, int32_t depth, VkFormat format,
            Wrapping wrapping, Filtering filtering) : width(width), height(height), depth(depth),
            wrapping(wrapping), filtering(filtering), format(format) {

            channels = int32_t(Graphics::GetFormatChannels(format));

            Reallocate(width, height, depth, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

        }

        void VulkanTexture::SetData(std::vector<uint8_t> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void VulkanTexture::SetData(std::vector<uint16_t> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void VulkanTexture::SetData(std::vector<float16> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void VulkanTexture::SetData(std::vector<float> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void VulkanTexture::GenerateMipmap() {

            // TODO...

        }

        void VulkanTexture::Reallocate(int32_t width, int32_t height, int32_t depth,
            Filtering filtering, Wrapping wrapping) {

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

            this->width = width;
            this->height = height;
            this->depth = depth;

            bool generateMipMaps = filtering == Filtering::MipMapLinear ||
                filtering == Filtering::MipMapNearest || filtering == Filtering::Anisotropic;

            VkImageUsageFlags additionalUsageFlags = {};
            if (generateMipMaps) {
                additionalUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }
            auto imageDesc = Graphics::ImageDesc {
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | additionalUsageFlags,
                .type = Graphics::ImageType::Image2D,
                .width = uint32_t(width),
                .height = uint32_t(height),
                .depth = uint32_t(depth),
                .format = format,
                .mipMapping = generateMipMaps,
            };
            image = graphicsDevice->CreateImage(imageDesc);

        }

        void VulkanTexture::RecreateSampler(Filtering filtering, Wrapping wrapping) {

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

            this->filtering = filtering;
            this->wrapping = wrapping;

            bool generateMipMaps = filtering == Filtering::MipMapLinear ||
                filtering == Filtering::MipMapNearest || filtering == Filtering::Anisotropic;
            bool anisotropicFiltering = filtering == Filtering::Anisotropic;

            VkFilter filter;
            switch (filtering) {
                case Filtering::Nearest: filter = VK_FILTER_NEAREST; break;
                case Filtering::MipMapNearest: filter = VK_FILTER_NEAREST; break;
                default: filter = VK_FILTER_LINEAR; break;
            }

            VkSamplerAddressMode mode;
            switch(wrapping) {
                case Wrapping::Repeat: mode = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
                case Wrapping::ClampToEdge: mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
                default: mode = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
            }

            VkSamplerMipmapMode mipmapMode;
            switch(filtering) {
                case Filtering::MipMapLinear: mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
                case Filtering::Anisotropic: mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
                default: mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
            }

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = filter,
                .mode = mode,
                .mipmapMode = mipmapMode,
                .maxLod = float(this->image->mipLevels),
                .anisotropicFiltering = generateMipMaps && anisotropicFiltering
            };
            sampler = graphicsDevice->CreateSampler(samplerDesc);

        }

    }

}