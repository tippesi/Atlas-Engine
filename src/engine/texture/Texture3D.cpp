#include "Texture3D.h"

namespace Atlas {

    namespace Texture {

        Texture3D::Texture3D(const Texture3D& that) {

            DeepCopy(that);

        }

        Texture3D::Texture3D(int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
                                       int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->width = width;
            this->height = height;
            this->depth = depth;

            Generate(GL_TEXTURE_3D, sizedFormat, wrapping, filtering,
                     anisotropicFiltering, generateMipMaps);

        }

        Texture3D& Texture3D::operator=(const Texture3D &that) {

            if (this != &that) {

                Texture::operator=(that);

            }

            return *this;

        }

        void Texture3D::Resize(int32_t width, int32_t height, int32_t depth) {

            if (width != this->width || height != this->height ||
                depth != this->depth) {

                this->width = width;
                this->height = height;
                this->depth = depth;

                glDeleteTextures(1, &ID);
                glGenTextures(1, &ID);

                Generate(GL_TEXTURE_3D, sizedFormat, wrapping,
                         filtering, anisotropicFiltering, mipmaps);

            }

        }

        void Texture3D::ReserveStorage(int32_t mipCount) {

            glTexStorage3D(GL_TEXTURE_3D, mipCount, sizedFormat, width, height, depth);

        }

    }

}