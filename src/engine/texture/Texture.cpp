#include "Texture.h"
#include "../Extensions.h"

#include <stb_image_resize.h>

namespace Atlas {

    namespace Texture {

        int32_t Texture::anisotropyLevel = 0;

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

		void Texture::Bind() const {

			if (!target)
				return;

			glBindTexture(target, ID);

		}

		void Texture::Bind(uint32_t unit) const {

			if (!target)
				return;

			glActiveTexture(unit);
			Bind();

		}

		void Texture::Bind(uint32_t access, uint32_t unit, int32_t level) const {

			glBindImageTexture(unit, ID, level, depth > 1, 0, access, sizedFormat);

		}

		void Texture::Unbind() const {

			if (!target)
				return;

			glBindTexture(target, 0);

		}

		void Texture::Unbind(uint32_t unit) const {

			if (!target)
				return;

			glActiveTexture(unit);
			Unbind();

		}

        bool Texture::IsValid() const {

            return width > 0 && height > 0 &&
                channels > 0 && depth > 0;

        }

		void Texture::SetBias(float bias) {

#ifdef AE_API_GL
			Bind();
			glTexParameterf(target, GL_TEXTURE_LOD_BIAS, bias);
			this->bias = bias;
#endif

		}

		float Texture::GetBias() const {

			return bias;

		}

        uint32_t Texture::GetID() const {

            return ID;

        }

        uint32_t Texture::GetDataType() const {

            return dataType;

        }

        int32_t Texture::GetSizedFormat() const {

            return sizedFormat;

        }

        void Texture::Copy(const Texture &texture) {

            if (width != texture.width || height != texture.height ||
                depth != texture.depth || channels != texture.channels)
                return;

			Copy(texture, 0, 0, 0, 0, 0, 0, width, height, depth);

        }

		void Texture::Copy(const Texture& texture, int32_t srcX, int32_t srcY,
			int32_t srcZ, int32_t destX, int32_t destY, int32_t destZ,
			int32_t width, int32_t height, int32_t depth) {

			if (width <= 0 || height <= 0 || depth <= 0 || texture.channels != channels ||
				srcX + width > texture.width || srcY + height > texture.height ||
				srcZ + depth > texture.depth || destX + width > this->width ||
				destY + height > this->height || destZ + depth > this->depth )
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

		int32_t Texture::GetMipMapLevel() const {

			return (int32_t)floor(log2(glm::max((float)width, (float)height))) + 1;

		}

        int32_t Texture::GetMaxAnisotropyLevel() {

            float maxAnisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);

            if (!anisotropyLevel)
                anisotropyLevel = (int32_t)maxAnisotropy;
            else
                anisotropyLevel = glm::min(anisotropyLevel, (int32_t)maxAnisotropy);

            return int32_t(maxAnisotropy);


        }

        void Texture::SetAnisotropyLevel(int32_t anisotropyLevel) {

            if (!Texture::anisotropyLevel)
                GetMaxAnisotropyLevel();

            if (Texture::anisotropyLevel) {
                float maxAnisotropy;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);

                Texture::anisotropyLevel = glm::min(anisotropyLevel, (int32_t)maxAnisotropy);
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

        void Texture::Unbind(uint32_t target, uint32_t unit) {

            glActiveTexture(unit);
            glBindTexture(target, 0);

        }

        void Texture::Generate(uint32_t target, int32_t sizedFormat, int32_t wrapping,
                               int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

			if (!target || !ID)
				return;

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
                    glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY, anisotropyLevel);
                }
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            else {
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filtering);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filtering);
            }

            glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapping);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapping);
            glTexParameteri(target, GL_TEXTURE_WRAP_R, wrapping);

			if (width > 0 && height > 0 && depth > 0)
				ReserveStorage(mipCount);

			Unbind();

        }

		void Texture::DeepCopy(const Texture& that) {

			width = that.width;
			height = that.height;
            depth = that.depth;

			Generate(that.target, that.sizedFormat, that.wrapping, that.filtering,
				that.anisotropicFiltering, that.mipmaps);

			Copy(that);

		}

    }

}