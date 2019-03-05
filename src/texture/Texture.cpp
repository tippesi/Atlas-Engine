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

            width = 0;
            height = 0;
            depth = 0;
            channels = 0;

            dataType = 0;
            sizedFormat = 0;
            mipmaps = false;

        }

        Texture::~Texture() {

            glDeleteTextures(1, &ID);

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

        void Texture::FlipDataHorizontally(std::vector <uint8_t> &data) {

            auto invertedData = std::vector<uint8_t>(width * height * channels);

            int32_t dataIndex = width * (height + 1) * channels;

            for (int32_t i = 0; i < width * height * channels; i++) {

                if (dataIndex % (width * channels) == 0) {
                    dataIndex = dataIndex - 2 * width * channels;
                }

                invertedData[dataIndex] = data[i];

                dataIndex++;

            }

            data.assign(invertedData.size(), *invertedData.data());

        }

        int32_t Texture::GetMipMapLevel() {

            return (int32_t)floor(log2(glm::max((float)width, (float)height))) + 1;

        }

        void Texture::Generate(GLenum target,int32_t sizedFormat, int32_t wrapping,
                               int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->channels = GetChannelCount(TextureFormat::GetBaseFormat(sizedFormat));
            this->dataType = TextureFormat::GetType(sizedFormat);
            this->sizedFormat = sizedFormat;
            this->wrapping = wrapping;
            this->filtering = filtering;
            this->anisotropicFiltering = anisotropicFiltering;
            this->mipmaps = generateMipMaps;

            glBindTexture(target, ID);

            int32_t mipCount = mipmaps ? GetMipMapLevel() : 1;

            ReserveStorage(mipCount);

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
            }

        }

    }

}