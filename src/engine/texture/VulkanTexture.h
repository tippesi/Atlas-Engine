#ifndef AE_VULKANTEXTURE_H
#define AE_VULKANTEXTURE_H

#include "../System.h"

#include "../graphics/Image.h"
#include "../graphics/Sampler.h"

namespace Atlas {

    namespace Texture {

        class VulkanTexture {

        public:
            VulkanTexture() = default;

            /**
              * Construct a Texture2DArray object.
              * @param width The width of the texture.
              * @param height The height of the texture.
              * @param depth The number of texture depth.
              * @param sizedFormat The sized texture format. See {@link TextureFormat.h} for more.
              * @param wrapping The wrapping of the texture. Controls texture border behaviour.
              * @param filtering The filtering of the texture.
              */
            VulkanTexture(int32_t width, int32_t height, int32_t depth, VkFormat format,
                VkSamplerAddressMode wrapping = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VkFilter filtering = VK_FILTER_NEAREST,
                bool anisotropicFiltering = false, bool generateMipMaps = false);

            /**
			 * Sets the data of the texture
			 * @param data A vector holding the new data.
			 */
            void SetData(std::vector<uint8_t>& data);

            /**
             * Sets the data of the texture
             * @param data A pointer to the data
             */
            void SetData(std::vector<uint16_t>& data);

            Graphics::Image* image;
            Graphics::Sampler* sampler;

        };

    }

}

#endif