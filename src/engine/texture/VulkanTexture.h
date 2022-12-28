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
            Repeat = 0,
            ClampToEdge
        };

        enum class Filtering {
            Nearest = 0,
            Linear,
            MipMapNearest,
            MipMapLinear,
            Anisotropic
        };

        class VulkanTexture {

        public:
            VulkanTexture() = default;

            /**
              * Construct a Texture2DArray object.
              * @param width The width of the texture.
              * @param height The height of the texture.
              * @param depth The number of texture depth.
              * @param sizedFormat The sized texture format.
              * @param wrapping The wrapping of the texture. Controls texture border behaviour.
              * @param filtering The filtering of the texture.
              */
            VulkanTexture(int32_t width, int32_t height, int32_t depth, VkFormat format,
                Wrapping wrapping = Wrapping::Repeat, Filtering filtering = Filtering::Nearest);

            /**
             * Validates a texture.
             * @return True, if the texture is considered valid, false otherwise
             * @note The validity is checked by checking the width,
             * height, channels and depth of the texture. No data is
             * validated with this method.
             */
            bool IsValid() const;

            /**
			 * Sets the data of the texture
			 * @param data A vector holding the new data.
			 */
            void SetData(std::vector<uint8_t>& data);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             */
            void SetData(std::vector<uint16_t>& data);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             */
            void SetData(std::vector<float16>& data);

            /**
             * Sets the data of the texture
             * @param data A vector holding the new data.
             */
            void SetData(std::vector<float>& data);

            /**
             * Retrieves the data of the texture from the GPU.
             * @param depth The depth where the data should be retrieved.
             * @return A vector holding the data.
             * @note Only uint8_t, uint16_t and float are supported typenames. Also
             * retrieving the data will stall all operations until previous commands
             * are done executing.
             */
            template<typename T> std::vector<T> GetData(int32_t depth = 0);

            void GenerateMipmap();

            Ref<Graphics::Image> image = nullptr;
            Ref<Graphics::Sampler> sampler = nullptr;

            int32_t width = 0;
            int32_t height = 0;
            int32_t depth = 1;
            int32_t channels = 1;

            Wrapping wrapping;
            Filtering filtering;

        protected:
            void SetData(void* data, int32_t x, int32_t y, int32_t z,
                int32_t width, int32_t height, int32_t depth);

            void Reallocate(Graphics::ImageType imageType, int32_t width, int32_t height,
                int32_t depth, Filtering filtering, Wrapping wrapping);

            void RecreateSampler(Filtering filtering, Wrapping wrapping);

            VkFormat format;

        };

        template<typename T>
        std::vector<T> VulkanTexture::GetData(int32_t depth) {

            static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                          std::is_same_v<T, float>, "Unsupported type. Supported are uint8_t, uint16_t and float");

            std::vector<T> data(width * height * channels);
            // Get data via memory transfer manager


            return data;

        }

    }

}

#endif