#include "Texture2D.h"

namespace Atlas {

    namespace Texture {

		Texture2D::Texture2D(const Texture2D& that) {

			DeepCopy(that);

		}

        Texture2D::Texture2D(int32_t width, int32_t height, int32_t sizedFormat, int32_t wrapping,
                             int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->width = width;
            this->height = height;
			this->depth = 1;

            Generate(GL_TEXTURE_2D, sizedFormat, wrapping, filtering, anisotropicFiltering, generateMipMaps);

        }

        Texture2D::Texture2D(std::string filename, bool colorSpaceConversion, bool anisotropicFiltering,
			bool generateMipMaps, int32_t forceChannels) {

			auto image = Loader::ImageLoader::LoadImage<uint8_t>(filename,
					colorSpaceConversion, forceChannels);
			InitializeInternal(image, anisotropicFiltering, generateMipMaps);

        }

		Texture2D::Texture2D(Common::Image<uint8_t>& image, bool anisotropicFiltering, bool generateMipMaps) {

			InitializeInternal(image, anisotropicFiltering, generateMipMaps);

		}

        Texture2D& Texture2D::operator=(const Texture2D &that) {

            if (this != &that) {

                Texture::operator=(that);

            }

            return *this;

        }

		void Texture2D::SetData(std::vector<uint8_t>& data) {

			Bind();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
				TextureFormat::GetBaseFormat(sizedFormat), AE_UBYTE, data.data());

			GenerateMipmap();

		}

        void Texture2D::SetData(std::vector<uint8_t> &data, int32_t mipLevel) {

			auto div = (int32_t)powf(2.0f, (float)mipLevel);
			auto width = this->width / div;
			auto height = this->height / div;

			Bind();
            glTexSubImage2D(GL_TEXTURE_2D, mipLevel, 0, 0, width, height,
				TextureFormat::GetBaseFormat(sizedFormat), AE_UBYTE, data.data());

        }

		void Texture2D::SetData(std::vector<uint16_t>& data) {

			Bind();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
				TextureFormat::GetBaseFormat(sizedFormat), AE_USHORT, data.data());

			GenerateMipmap();

		}

        void Texture2D::SetData(std::vector<uint16_t> &data, int32_t mipLevel) {

			auto div = (int32_t)powf(2.0f, (float)mipLevel);
			auto width = this->width / div;
			auto height = this->height / div;

			Bind();
            glTexSubImage2D(GL_TEXTURE_2D, mipLevel, 0, 0, width, height,
				TextureFormat::GetBaseFormat(sizedFormat), AE_USHORT, data.data());

        }

        void Texture2D::SetData(std::vector<float16>& data) {

            Bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                TextureFormat::GetBaseFormat(sizedFormat), AE_HALF_FLOAT, data.data());

            GenerateMipmap();

        }

		void Texture2D::SetData(std::vector<float>& data) {

			Bind();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
				TextureFormat::GetBaseFormat(sizedFormat), AE_FLOAT, data.data());

			GenerateMipmap();

		}

        void Texture2D::Resize(int32_t width, int32_t height) {

			if (width != this->width || height != this->height) {

				this->width = width;
				this->height = height;

				glDeleteTextures(1, &ID);
				glGenTextures(1, &ID);

				Generate(GL_TEXTURE_2D, sizedFormat, wrapping, filtering, anisotropicFiltering, mipmaps);

			}

        }

        void Texture2D::ReserveStorage(int32_t mipCount) {

            glTexStorage2D(GL_TEXTURE_2D, mipCount, sizedFormat, width, height);

        }

        void Texture2D::InitializeInternal(Atlas::Common::Image<uint8_t> &image,
        	bool anisotropicFilterin, bool generateMipMaps) {

			int32_t sizedFormat;

			switch (image.channels) {
				case 4: sizedFormat = AE_RGBA8; break;
				case 3: sizedFormat = AE_RGB8; break;
				case 2: sizedFormat = AE_RG8; break;
				case 1: sizedFormat = AE_R8; break;
			}

			width = image.width;
			height = image.height;
			channels = image.channels;
			this->depth = 1;

			Generate(GL_TEXTURE_2D, sizedFormat, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR,
					 anisotropicFiltering, generateMipMaps);

			SetData(image.GetData());

		}

    }

}