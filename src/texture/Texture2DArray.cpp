#include "Texture2DArray.h"

#include "../Framebuffer.h"
#include "../loader/ImageLoader.h"

namespace Atlas {

    namespace Texture {

		Texture2DArray::Texture2DArray(const Texture2DArray& that) {

			DeepCopy(that);

		}

        Texture2DArray::Texture2DArray(int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
                                       int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->width = width;
            this->height = height;
            this->layers = depth;

            Generate(GL_TEXTURE_2D_ARRAY, sizedFormat, wrapping, filtering,
                     anisotropicFiltering, generateMipMaps);

        }

		Texture2DArray& Texture2DArray::operator=(const Texture2DArray &that) {

			if (this != &that) {

				Texture::operator=(that);

			}

			return *this;

		}

        void Texture2DArray::SetData(std::vector<uint8_t> &data, int32_t depth, int32_t count) {

			SetData(data, 0, 0, depth, width, height, count);			

        }

		void Texture2DArray::SetData(std::vector<uint8_t>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t depth) {

			Bind();
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, z, width, height, depth,
				TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

			GenerateMipmap();

		}

        void Texture2DArray::SetData(std::vector<uint16_t> &data, int32_t depth, int32_t count) {

			SetData(data, 0, 0, depth, width, height, count);

        }

		void Texture2DArray::SetData(std::vector<uint16_t>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t depth) {

			Bind();
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, z, width, height, depth,
				TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

			GenerateMipmap();

		}

        std::vector<uint8_t> Texture2DArray::GetData(int32_t depth) {

            auto framebuffer = Framebuffer(width, height);

            std::vector<uint8_t> data(width * height * channels * TypeFormat::GetSize(dataType));

            framebuffer.AddComponentTextureArray(GL_COLOR_ATTACHMENT0, this, depth);

            glReadPixels(0, 0, width, height,
                    TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

            framebuffer.Unbind();

            return data;

        }

        void Texture2DArray::Resize(int32_t width, int32_t height, int32_t depth) {

            this->width = width;
            this->height = height;
            this->layers = depth;

            glDeleteTextures(1, &ID);
            glGenTextures(1, &ID);

            Generate(GL_TEXTURE_2D_ARRAY, sizedFormat, wrapping,
                     filtering, anisotropicFiltering, mipmaps);

        }

        void Texture2DArray::SaveToPNG(std::string filename, int32_t depth) {

			Common::Image8 image;

            image.width = width;
            image.height = height;
            image.fileFormat = AE_IMAGE_PNG;

            image.data = GetData(depth);
            FlipDataHorizontally(image.data);

            Loader::ImageLoader::SaveImage(image, filename);

        }

        void Texture2DArray::ReserveStorage(int32_t mipCount) {

            glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipCount, sizedFormat, width, height, layers);

        }

    }

}