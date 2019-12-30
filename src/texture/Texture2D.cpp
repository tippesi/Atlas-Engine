#include "Texture2D.h"
#include "../Framebuffer.h"
#include "../loader/ImageLoader.h"

namespace Atlas {

    namespace Texture {

		Texture2D::Texture2D(const Texture2D& that) {

			DeepCopy(that);

		}

        Texture2D::Texture2D(int32_t width, int32_t height, int32_t sizedFormat, int32_t wrapping,
                             int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->width = width;
            this->height = height;
			this->layers = 1;

            Generate(GL_TEXTURE_2D, sizedFormat, wrapping, filtering, anisotropicFiltering, generateMipMaps);

        }

        Texture2D::Texture2D(std::string filename, bool colorSpaceConversion, bool anisotropicFiltering,
                             bool generateMipMaps, int32_t forceChannels) {

            auto image = Loader::ImageLoader::LoadImage(filename, colorSpaceConversion, forceChannels);

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
			this->layers = 1;

            Generate(GL_TEXTURE_2D, sizedFormat, GL_CLAMP_TO_EDGE, GL_LINEAR,
                     anisotropicFiltering, generateMipMaps);

            SetData(image.GetData());

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

        std::vector<uint8_t> Texture2D::GetData() {

            auto framebuffer = Framebuffer(width, height);

            std::vector<uint8_t> data(width * height * channels * TypeFormat::GetSize(dataType));

			if (TextureFormat::GetBaseFormat(sizedFormat) != AE_DEPTH)
				framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, this);
			else
				framebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, this);

            glReadPixels(0, 0, width, height,
                    TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

            framebuffer.Unbind();

            return data;

        }

        void Texture2D::Resize(int32_t width, int32_t height) {

            this->width = width;
            this->height = height;

            glDeleteTextures(1, &ID);
            glGenTextures(1, &ID);

            Generate(GL_TEXTURE_2D, sizedFormat, wrapping, filtering, anisotropicFiltering, mipmaps);

        }

        void Texture2D::SaveToPNG(std::string filename, bool flipHorizontally) {

            Common::Image8 image;

            image.width = width;
            image.height = height;
			image.channels = channels;
            image.fileFormat = AE_IMAGE_PNG;

            auto data = GetData();

			if (flipHorizontally)
				data = FlipDataHorizontally(data);

			image.SetData(data);

            Loader::ImageLoader::SaveImage(image, filename);

        }

        void Texture2D::ReserveStorage(int32_t mipCount) {

            glTexStorage2D(GL_TEXTURE_2D, mipCount, sizedFormat, width, height);

        }

    }

}