#include "VulkanTexture.h"

#include "../graphics/Instance.h"
#include "../graphics/Format.h"

namespace Atlas {

    namespace Texture {

        VulkanTexture::VulkanTexture(int32_t width, int32_t height, int32_t depth, VkFormat format,
            Wrapping wrapping, Filtering filtering) : width(width), height(height), depth(depth),
            wrapping(wrapping), filtering(filtering), format(format) {

            channels = int32_t(Graphics::GetFormatChannels(format));

            Reallocate(Graphics::ImageType::Image2D, width, height, depth, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

        }

        bool VulkanTexture::IsValid() const {

            return width > 0 && height > 0 &&
                   channels > 0 && depth > 0;

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

        void VulkanTexture::SetData(void* data, int32_t x, int32_t y, int32_t z,
            int32_t width, int32_t height, int32_t depth) {

            if (image->type == Graphics::ImageType::Image1DArray ||
                image->type == Graphics::ImageType::Image2DArray ||
                image->type == Graphics::ImageType::ImageCube)   {
                image->SetData(data, uint32_t(x), uint32_t(y), 0,
                    uint32_t(width), uint32_t(height), 1,
                    uint32_t(z), uint32_t(depth));
            }
            else {
                image->SetData(data, uint32_t(x), uint32_t(y), uint32_t(z),
                    uint32_t(width), uint32_t(height), uint32_t(depth));
            }

        }

        void VulkanTexture::Reallocate(Graphics::ImageType imageType, int32_t width, int32_t height, int32_t depth,
            Filtering filtering, Wrapping wrapping) {

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

            this->width = width;
            this->height = height;
            this->depth = depth;

            bool generateMipMaps = filtering == Filtering::MipMapLinear ||
                filtering == Filtering::MipMapNearest || filtering == Filtering::Anisotropic;
            bool depthFormat = format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM ||
                format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;

            VkImageUsageFlags additionalUsageFlags = {};
            if (generateMipMaps) {
                additionalUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }
            // We assume this texture was generated not for exclusive, but e.g. as framebuffer/storage texture
            if (!generateMipMaps) {
                if (depthFormat) {
                    additionalUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                }
                else {
                    additionalUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                    additionalUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
                }
            }

            auto arrayType = imageType == Graphics::ImageType::Image2DArray ||
                imageType == Graphics::ImageType::Image1DArray ||
                imageType == Graphics::ImageType::ImageCube;

            VkImageAspectFlags aspectFlag = depthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

            auto imageDesc = Graphics::ImageDesc {
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | additionalUsageFlags,
                .type = imageType,
                .aspectFlags = aspectFlag,
                .width = uint32_t(width),
                .height = uint32_t(height),
                .depth = arrayType ? 1 : uint32_t(depth),
                .layers = arrayType ? uint32_t(depth) : 1,
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