#include "Texture.h"

#include "../libraries/stb/stb_image.h"
#include "../libraries/stb/stb_image_write.h"
#include "../libraries/stb/stb_image_resize.h"

int32_t Texture::anisotropyLevel = 0;

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

    int32_t extensionCount = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

    for (int32_t i = 0; i < extensionCount; i++) {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(extension, "GL_EXT_texture_filter_anisotropic")) {

            float maxAnisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

            if (!anisotropyLevel)
                anisotropyLevel = (int32_t)maxAnisotropy;
            else
                anisotropyLevel = glm::min(anisotropyLevel, (int32_t)maxAnisotropy);

            return (int32_t)maxAnisotropy;
        }
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

void Texture::GammaToLinear(vector <uint8_t> &data, int32_t width, int32_t height, int32_t channels) {

    for (int32_t i = 0; i < width * height * channels; i++) {
        // Don't correct the alpha values
        if (channels == 4 && (i + 1) % 4 == 0)
            continue;

        // Using OpenGL conversion:
        float value = (float)data[i] / 255.0f;
        value = value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
        // Before we can uncorrect it we have to bring it in normalized space
        data[i] = (uint8_t)(glm::clamp(value, 0.0f, 1.0f) * 255.0f);

    }

}

int32_t Texture::GetBaseFormat(int32_t sizedFormat) {

    // See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
    switch (sizedFormat) {
        case GL_R8: return GL_RED;
        case GL_R8_SNORM: return GL_RED;
        case GL_R16: return GL_RED;
        case GL_R16_SNORM: return GL_RED;
        case GL_RG8: return GL_RG;
        case GL_RG8_SNORM: return GL_RG;
        case GL_RG16: return GL_RG;
        case GL_RG16_SNORM: return GL_RG;
        case GL_RGB5_A1: return GL_RGBA;
        case GL_RGBA8: return GL_RGBA;
        case GL_RGBA8_SNORM: return GL_RGBA;
        case GL_RGB10_A2: return GL_RGBA;
        case GL_RGB10_A2UI: return GL_RGBA;
        case GL_RGBA12: return GL_RGBA;
        case GL_RGBA16: return GL_RGBA;
        case GL_SRGB8_ALPHA8: return GL_RGBA;
        case GL_R16F: return GL_RED;
        case GL_RG16F: return GL_RG;
        case GL_RGBA16F: return GL_RGBA;
        case GL_R32F: return GL_RED;
        case GL_RG32F: return GL_RG;
        case GL_RGBA32F: return GL_RGBA;
        case GL_DEPTH_COMPONENT16: return GL_DEPTH_COMPONENT;
        case GL_DEPTH_COMPONENT24: return GL_DEPTH_COMPONENT;
        case GL_DEPTH_COMPONENT32F: return GL_DEPTH_COMPONENT;
        case GL_DEPTH24_STENCIL8: return GL_DEPTH_STENCIL;
        case GL_DEPTH32F_STENCIL8: return GL_DEPTH_STENCIL;
        default: return GL_RGB;
    }

}

void Texture::FlipDataHorizontally(vector <uint8_t> &data) {

    auto invertedData = vector<uint8_t>(width * height * channels);

    int32_t dataIndex = width * (height + 1) * channels;

    for (int32_t i = 0; i < width * height * channels; i++) {

        if (dataIndex % (width * channels) == 0) {
            dataIndex = dataIndex - 2 * width * channels;
        }

        invertedData[dataIndex] = data[i];

        dataIndex++;

    }

    data.assign(invertedData.size(), invertedData.data());

}

void Texture::Generate(GLenum target, GLenum dataType, int32_t sizedFormat, int32_t wrapping,
        int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

    glBindTexture(target, ID);

    int32_t mipCount = mipmaps ? (int32_t)floor(log2(glm::max((float)width, (float)height))) + 1 : 1;

    ReserveStorage(mipCount);

    if (generateMipMaps) {
        glGenerateMipmap(target);
        if (anisotropicFiltering) {
            glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropyLevel);
        }
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    this->channels = GetChannelCount(GetBaseFormat(sizedFormat));
    this->sizedFormat = sizedFormat;
    this->mipmaps = generateMipMaps;

}