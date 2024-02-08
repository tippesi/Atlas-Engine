#include "Texture.h"

#include "../graphics/Instance.h"
#include "../graphics/Format.h"

#include "../common/Hash.h"

#include <mutex>

namespace Atlas {

    namespace Texture {

        std::unordered_map<size_t, Ref<Graphics::Sampler>> Texture::samplerMap;

        Texture::Texture(int32_t width, int32_t height, int32_t depth, VkFormat format,
            Wrapping wrapping, Filtering filtering) : width(width), height(height), depth(depth),
            wrapping(wrapping), filtering(filtering), format(format) {

            Reallocate(Graphics::ImageType::Image2D, width, height, depth, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

        }

        void Texture::Bind(Graphics::CommandList *commandList, uint32_t set, uint32_t binding) {

            commandList->BindImage(image, sampler, set, binding);

        }

        bool Texture::IsValid() const {

            return width > 0 && height > 0 && channels > 0 && depth > 0 && image != nullptr;

        }

        void Texture::SetData(std::vector<uint8_t> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void Texture::SetData(std::vector<uint16_t> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void Texture::SetData(std::vector<float16> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void Texture::SetData(std::vector<float> &data) {

            image->SetData(data.data(), 0, 0, 0, size_t(width), size_t(height), size_t(depth));

        }

        void Texture::GenerateMipmap() {

            // TODO...

        }

        void Texture::SetData(void* data, int32_t x, int32_t y, int32_t z,
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

        void Texture::Reallocate(Graphics::ImageType imageType, int32_t width, int32_t height, int32_t depth,
            Filtering filtering, Wrapping wrapping) {

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

            this->width = width;
            this->height = height;
            this->depth = depth;
            channels = int32_t(Graphics::GetFormatChannels(format));

            bool generateMipMaps = filtering == Filtering::MipMapLinear ||
                filtering == Filtering::MipMapNearest || filtering == Filtering::Anisotropic;
            bool depthFormat = format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM ||
                format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;

            VkImageUsageFlags additionalUsageFlags = {};
            // We assume this texture was generated not for exclusive, but e.g. as framebuffer/storage texture
            if (depthFormat) {
                additionalUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            else {
                additionalUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                additionalUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
            }

            auto arrayType = imageType == Graphics::ImageType::Image2DArray ||
                imageType == Graphics::ImageType::Image1DArray ||
                imageType == Graphics::ImageType::ImageCube;

            VkImageAspectFlags aspectFlag = depthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

            auto imageDesc = Graphics::ImageDesc {
                .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
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

        void Texture::RecreateSampler(Filtering filtering, Wrapping wrapping) {

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
            VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            switch(wrapping) {
                case Wrapping::Repeat: mode = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
                case Wrapping::ClampToEdge: mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
                case Wrapping::ClampToWhite: mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
                case Wrapping::ClampToBlack: mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
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
                .borderColor = borderColor,
                .anisotropicFiltering = generateMipMaps && anisotropicFiltering
            };
            sampler = GetOrCreateSampler(samplerDesc);

        }

        void Texture::Shutdown() {

            samplerMap.clear();

        }

        Ref<Graphics::Sampler> Texture::GetOrCreateSampler(Graphics::SamplerDesc desc) {

            static std::mutex mutex;

            size_t hash = 0;

            // We can only have different variants of samplers up to the device limit (e.g. 4000 on Nvidia cards)
            HashCombine(hash, desc.filter);
            HashCombine(hash, desc.mode);
            HashCombine(hash, desc.mipmapMode);
            HashCombine(hash, desc.maxLod);
            HashCombine(hash, desc.anisotropicFiltering);

            std::lock_guard guard(mutex);

            if (samplerMap.contains(hash))
                return samplerMap[hash];

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;
            
            auto sampler = graphicsDevice->CreateSampler(desc);
            samplerMap[hash] = sampler;

            return sampler;

        }

    }

}