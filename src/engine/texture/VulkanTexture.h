#ifndef AE_VULKANTEXTURE_H
#define AE_VULKANTEXTURE_H

#include "../System.h"

#include "../common/Image.h"
#include "../loader/ImageLoader.h"

#include "../common/Ref.h"
#include "../graphics/Image.h"
#include "../graphics/Sampler.h"

namespace Atlas {

    namespace Texture {

        enum class Wrapping {

        };

        enum class Filtering {

        };

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
            VulkanTexture(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                VkSamplerAddressMode wrapping = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VkFilter filtering = VK_FILTER_NEAREST,
                bool anisotropicFiltering = false, bool generateMipMaps = false);

            /**
			 * Constructs a Texture2D object from an image object.
			 * @param image The image object.
			 * @param anisotropicFiltering Whether or not anisotropic filtering is used.
			 * @param generateMipMaps Whether or not mipmap can be used. Generate using GenerateMipmap()
			 */
            explicit VulkanTexture(Common::Image<uint8_t>& image, bool anisotropicFiltering = true,
                bool generateMipMaps = true);

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

            Ref<Graphics::Image> image = nullptr;
            Ref<Graphics::Sampler> sampler = nullptr;

            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t depth = 1;

        };

    }

}

#endif