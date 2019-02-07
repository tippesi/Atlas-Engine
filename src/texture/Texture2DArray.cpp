#include "Texture2DArray.h"

#include "../Framebuffer.h"
#include "../loader/ImageLoader.h"

namespace Atlas {

    namespace Texture {

        Texture2DArray::Texture2DArray(GLenum dataType, int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
                                       int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

            this->width = width;
            this->height = height;
            this->depth = depth;

            Generate(GL_TEXTURE_2D_ARRAY, dataType, sizedFormat, wrapping, filtering,
                     anisotropicFiltering, generateMipMaps);

        }

        void Texture2DArray::Bind(uint32_t unit) {

            glActiveTexture(unit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, ID);

        }

        void Texture2DArray::Unbind() {

            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

        }

        void Texture2DArray::SetData(std::vector<uint8_t> &data, int32_t depth, int32_t count) {

            glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, depth, width, height, count,
                            TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());
            if (mipmaps)
                glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        }

        void Texture2DArray::SetData(std::vector<uint16_t> &data, int32_t depth, int32_t count) {

            glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, depth, width, height, count,
                            TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());
            if (mipmaps)
                glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        }

        std::vector<uint8_t> Texture2DArray::GetData(int32_t depth) {

            auto framebuffer = Framebuffer(width, height);

            std::vector<uint8_t> data(width * height * channels);

            framebuffer.AddComponentTextureArray(GL_COLOR_ATTACHMENT0, this, depth);

            glReadPixels(0, 0, width, height,
                         TextureFormat::GetBaseFormat(sizedFormat), GL_UNSIGNED_BYTE, data.data());

            framebuffer.Unbind();

            return data;

        }

        void Texture2DArray::Resize(int32_t width, int32_t height, int32_t depth) {

            this->width = width;
            this->height = height;
            this->depth = depth;

            glDeleteTextures(1, &ID);
            glGenTextures(1, &ID);

            Generate(GL_TEXTURE_2D_ARRAY, dataType, sizedFormat, wrapping,
                     filtering, anisotropicFiltering, mipmaps);

        }

        void Texture2DArray::SaveToPNG(std::string filename, int32_t depth) {

            Loader::Image image;

            image.width = width;
            image.height = height;
            image.fileFormat = AE_IMAGE_PNG;

            image.data = GetData(depth);
            FlipDataHorizontally(image.data);

            Loader::ImageLoader::SaveImage(image, filename);

        }

        void Texture2DArray::ReserveStorage(int32_t mipCount) {

            glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipCount, sizedFormat, width, height, depth);

        }

    }

}