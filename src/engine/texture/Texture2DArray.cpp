#include "Texture2DArray.h"

#include "../Framebuffer.h"
#include "../loader/ImageLoader.h"

namespace Atlas {

    namespace Texture {

        Texture2DArray::Texture2DArray(int32_t width, int32_t height, int32_t layers, VkFormat format,
            Wrapping wrapping, Filtering filtering) {

            this->format = format;
            Reallocate(width, height, layers, filtering, wrapping);
            RecreateSampler(filtering, wrapping);

        }

        void Texture2DArray::SetData(std::vector<uint8_t> &data, int32_t layer, int32_t count) {

			SetData(data, 0, 0, layer, width, height, count);

        }

		void Texture2DArray::SetData(std::vector<uint8_t>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t layers) {

            SetData(data, x, y, z, width, height, layers);

		}

        void Texture2DArray::SetData(std::vector<uint16_t> &data, int32_t layer, int32_t count) {

			SetData(data, 0, 0, layer, width, height, count);

        }

		void Texture2DArray::SetData(std::vector<uint16_t>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t layers) {

            SetData(data, x, y, z, width, height, layers);

		}

		void Texture2DArray::SetData(std::vector<float>& data, int32_t layer, int32_t count) {

			SetData(data, 0, 0, layer, width, height, count);

		}

		void Texture2DArray::SetData(std::vector<float>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t layers) {

            SetData(data, x, y, z, width, height, layers);

		}

        void Texture2DArray::Resize(int32_t width, int32_t height, int32_t layers) {

			if (width != this->width || height != this->height ||
                layers != this->depth) {

                Reallocate(width, height, layers, filtering, wrapping);

			}

        }

        void Texture2DArray::SaveToPNG(std::string filename, int32_t depth) {



        }

    }

}