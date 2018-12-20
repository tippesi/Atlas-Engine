#include "Texture2D.h"
#include "../Framebuffer.h"
#include "../loader/ImageLoader.h"

Texture2D::Texture2D(GLenum dataType, int32_t width, int32_t height, int32_t sizedFormat, int32_t wrapping,
        int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

    this->width = width;
    this->height = height;

    Generate(GL_TEXTURE_2D, dataType, sizedFormat, wrapping, filtering, anisotropicFiltering, generateMipMaps);

}

Texture2D::Texture2D(string filename, bool colorSpaceConversion, bool anisotropicFiltering,
        bool generateMipMaps) {

    Image image = ImageLoader::LoadImage(filename, colorSpaceConversion);

    int32_t sizedFormat;

    switch (image.channels) {
        case 4: sizedFormat = GL_RGBA8; break;
        case 3: sizedFormat = GL_RGB8; break;
        case 2: sizedFormat = GL_RG8; break;
        case 1: sizedFormat = GL_R8; break;
    }

    width = image.width;
    height = image.height;
    channels = image.channels;

    Generate(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, sizedFormat, GL_CLAMP_TO_EDGE, GL_LINEAR,
            anisotropicFiltering, generateMipMaps);

    SetData(image.data);

}

void Texture2D::Bind(uint32_t unit) {

    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, ID);

}

void Texture2D::Unbind() {

    glBindTexture(GL_TEXTURE_2D, 0);

}

void Texture2D::SetData(vector<uint8_t> &data) {

    glBindTexture(GL_TEXTURE_2D, ID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GetBaseFormat(sizedFormat), dataType, data.data());

    if (mipmaps)
        glGenerateMipmap(GL_TEXTURE_2D);

}

vector<uint8_t> Texture2D::GetData() {

    auto framebuffer = Framebuffer(width, height);

    vector<uint8_t> data = vector<uint8_t>(width * height * channels);

    framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, this);

    glReadPixels(0, 0, width, height, GetBaseFormat(sizedFormat), GL_UNSIGNED_BYTE, data.data());

    framebuffer.DeleteContent();

    framebuffer.Unbind();

    return data;

}

void Texture2D::Resize(int32_t width, int32_t height) {

    this->width = width;
    this->height = height;

    glDeleteTextures(1, &ID);
    glGenTextures(1, &ID);

    Generate(GL_TEXTURE_2D, dataType, sizedFormat, wrapping, filtering, anisotropicFiltering, mipmaps);

}

void Texture2D::SaveToPNG(string filename) {

    Image image;

    image.width = width;
    image.height = height;
    image.fileFormat = IMAGE_PNG;

    image.data = GetData();
    FlipDataHorizontally(image.data);

    ImageLoader::SaveImage(filename, image);

}

void Texture2D::ReserveStorage(int32_t mipCount) {

    glTexStorage2D(GL_TEXTURE_2D, mipCount, sizedFormat, width, height);

}