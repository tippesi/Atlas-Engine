#include "Texture.h"

#include "../libraries/stb/stb_image.h"
#include "../libraries/stb/stb_image_write.h"
#include "../libraries/stb/stb_image_resize.h"

namespace Atlas {

    namespace Texture {

        int32_t Texture::anisotropyLevel = 0;
        bool Texture::anisotropicFilteringSupported = false;

        Texture::Texture() {

            glGenTextures(1, &ID);

        }

        Texture::~Texture() {

            glDeleteTextures(1, &ID);

        }

        Texture& Texture::operator=(const Texture &that) {

            if (this != &that) {

                // Remember that we have persistent texture memory.
                // We can't just change the format, but are required to
                // retrieve a new texture ID.
                glDeleteTextures(1, &ID);
                glGenTextures(1, &ID);

				DeepCopy(that);

            }

            return *this;

        }

		void Texture::Bind() {

			glBindTexture(target, ID);

		}

		void Texture::Bind(uint32_t unit) {

			glActiveTexture(unit);
			Bind();

		}

		void Texture::Bind(uint32_t access, uint32_t unit, int32_t level) {

			glBindImageTexture(unit, ID, level, layers > 1, 0, access, sizedFormat);

		}

		void Texture::Unbind() {

			glBindTexture(target, 0);

		}

        uint32_t Texture::GetID() {

            return ID;

        }

        uint32_t Texture::GetDataType() {

            return dataType;

        }

        int32_t Texture::GetSizedFormat() {

            return sizedFormat;

        }

        void Texture::Copy(const Texture &texture) {

            if (width != texture.width || height != texture.height ||
                layers != texture.layers || channels != texture.channels)
                return;

			Copy(texture, 0, 0, 0, 0, 0, 0, width, height, layers);

        }

		void Texture::Copy(const Texture& texture, int32_t srcX, int32_t srcY,
			int32_t srcZ, int32_t destX, int32_t destY, int32_t destZ,
			int32_t width, int32_t height, int32_t depth) {

			if (width <= 0 || height <= 0 || depth <= 0 ||
				srcX + width > texture.width || srcY + height > texture.height ||
				srcZ + depth > texture.layers || destX + width > this->width || 
				destY + height > this->height || destZ + depth > this->layers)
				return;

			Bind();

			glCopyImageSubData(texture.ID, texture.target, 
				0, srcX, srcY, srcZ, ID, target,
				0, destX, destY, destZ,
				width, height, depth);

			GenerateMipmap();

		}

		void Texture::GenerateMipmap() {

			if (mipmaps)
				glGenerateMipmap(target);

		}

        int32_t Texture::GetMaxAnisotropyLevel() {

            if (anisotropicFilteringSupported) {

                float maxAnisotropy;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

                if (!anisotropyLevel)
                    anisotropyLevel = (int32_t)maxAnisotropy;
                else
                    anisotropyLevel = glm::min(anisotropyLevel, (int32_t)maxAnisotropy);

                return (int32_t)maxAnisotropy;

            }

            return 0;

        }

        void Texture::SetAnisotropyLevel(int32_t anisotropyLevel) {

            if (!Texture::anisotropyLevel)
                GetMaxAnisotropyLevel();

            if (Texture::anisotropyLevel) {
                float maxAnisotropy;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

                Texture::anisotropyLevel = glm::min(anisotropyLevel, (int32_t)maxAnisotropy);
            }

        }

        void Texture::GammaToLinear(uint8_t* data, int32_t width, int32_t height, int32_t channels) {

            int32_t size = width * height * channels;

            for (int32_t i = 0; i < size; i++) {
                // Don't correct the alpha values
                if (channels == 4 && (i + 1) % 4 == 0)
                    continue;

                float value = (float)data[i] / 255.0f;
                value = powf(value, 2.2f);
                // Before we can uncorrect it we have to bring it in normalized space
                data[i] = (uint8_t)(value * 255.0f);

            }

        }

        void Texture::GammaToLinear(uint16_t* data, int32_t width, int32_t height, int32_t channels) {

            int32_t size = width * height * channels;

            for (int32_t i = 0; i < size; i++) {
                // Don't correct the alpha values
                if (channels == 4 && (i + 1) % 4 == 0)
                    continue;

                float value = (float)data[i] / 65535.0f;
                value = powf(value, 2.2f);
                // Before we can uncorrect it we have to bring it in normalized space
                data[i] = (uint8_t)(value * 65535.0f);

            }

        }

        int32_t Texture::GetChannelCount(int32_t baseFormat) {

            switch(baseFormat) {
                case AE_RGBA: return 4;
				case AE_RGBA_INT: return 4;
                case AE_RG: return 2;
				case AE_RG_INT: return 2;
                case AE_R: return 1;
				case AE_R_INT: return 1;
                case AE_DEPTH: return 1;
                default: return 3;
            }

        }

        int32_t Texture::GetSuggestedFormat(int32_t channelCount) {

            switch(channelCount) {
                case 1: return AE_R8;
                case 2: return AE_RG8;
                case 4: return AE_RGBA8;
                default: return AE_RGB8;
            }

        }

        void Texture::CheckExtensions() {

            int32_t extensionCount = 0;
            glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

            for (int32_t i = 0; i < extensionCount; i++) {
                const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
                if (strcmp(extension, "GL_EXT_texture_filter_anisotropic") == 0) {
                    anisotropicFilteringSupported = true;
                }
            }

        }

		std::vector<uint8_t> Texture::FlipDataHorizontally(std::vector <uint8_t> &data) {

            auto invertedData = std::vector<uint8_t>(width * height * channels);

            int32_t dataIndex = width * (height - 1) * channels + channels;

            for (int32_t i = 0; i < width * height * channels - channels; i++) {

                if (dataIndex % (width * channels) == 0) {
                    dataIndex = dataIndex - 2 * width * channels;
                }

                invertedData[dataIndex] = data[i];

                dataIndex++;

            }

			return invertedData;

        }

        int32_t Texture::GetMipMapLevel() {

            return (int32_t)floor(log2(glm::max((float)width, (float)height))) + 1;

        }

        void Texture::Generate(uint32_t target, int32_t sizedFormat, int32_t wrapping,
                               int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->target = target;
            this->channels = GetChannelCount(TextureFormat::GetBaseFormat(sizedFormat));
            this->dataType = TextureFormat::GetType(sizedFormat);
            this->sizedFormat = sizedFormat;
            this->wrapping = wrapping;
            this->filtering = filtering;
            this->anisotropicFiltering = anisotropicFiltering;
            this->mipmaps = generateMipMaps;

            glBindTexture(target, ID);

            int32_t mipCount = mipmaps ? GetMipMapLevel() : 1;

			glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);

            if (generateMipMaps) {
                if (anisotropicFiltering) {
                    glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropyLevel);
                }
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            else {
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filtering);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filtering);

				glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapping);
				glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapping);
				glTexParameteri(target, GL_TEXTURE_WRAP_R, wrapping);
            }

			ReserveStorage(mipCount);

			Unbind();

        }

		void Texture::DeepCopy(const Texture& that) {

			this->width = that.width;
			this->height = that.height;
			this->layers = that.layers;

			Generate(that.target, that.sizedFormat, that.wrapping, that.filtering,
				that.anisotropicFiltering, that.mipmaps);

			Copy(that);

		}

    }

}