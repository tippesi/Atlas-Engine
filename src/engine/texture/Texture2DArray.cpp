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
				TextureFormat::GetBaseFormat(sizedFormat), AE_UBYTE, data.data());

			GenerateMipmap();

		}

        void Texture2DArray::SetData(std::vector<uint16_t> &data, int32_t depth, int32_t count) {

			SetData(data, 0, 0, depth, width, height, count);

        }

		void Texture2DArray::SetData(std::vector<uint16_t>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t depth) {

			Bind();
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, z, width, height, depth,
				TextureFormat::GetBaseFormat(sizedFormat), AE_USHORT, data.data());

			GenerateMipmap();

		}

		void Texture2DArray::SetData(std::vector<float>& data, int32_t depth, int32_t count) {

			SetData(data, 0, 0, depth, width, height, count);

		}

		void Texture2DArray::SetData(std::vector<float>& data, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t depth) {

			Bind();
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, z, width, height, depth,
				TextureFormat::GetBaseFormat(sizedFormat), AE_FLOAT, data.data());

			GenerateMipmap();

		}

        void Texture2DArray::Resize(int32_t width, int32_t height, int32_t depth) {

			if (width != this->width || height != this->height ||
				depth != this->layers) {

				this->width = width;
				this->height = height;
				this->layers = depth;

				glDeleteTextures(1, &ID);
				glGenTextures(1, &ID);

				Generate(GL_TEXTURE_2D_ARRAY, sizedFormat, wrapping,
					filtering, anisotropicFiltering, mipmaps);

			}

        }

        void Texture2DArray::SaveToPNG(std::string filename, int32_t depth) {

			Common::Image<uint8_t> image(width, height, channels);
            image.fileFormat = AE_IMAGE_PNG;

			image.SetData(GetData<uint8_t>(depth));
			image.FlipHorizontally();

            Loader::ImageLoader::SaveImage(image, filename);

        }

        void Texture2DArray::ReserveStorage(int32_t mipCount) {

            glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipCount, sizedFormat, width, height, layers);

        }

    }

}