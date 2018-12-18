#include "Texture2DArray.h"

Texture2DArray::Texture2DArray(GLenum dataType, int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
        int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) {

    Generate(GL_TEXTURE2D_ARRAY, dataType, sizedFormat, wrapping, filtering, anisotropicFiltering, generateMipMaps);

}

void Texture2DArray::Bind(uint32_t unit) {

    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ID);

}

void Texture2DArray::Unbind() {

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

}

void Texture2DArray::SetData(vector <uint8_t> &data, int32_t depth, int32_t count) {

    glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, depth, width, height, count,
            GetBaseFormat(sizedFormat), dataType, data.data());
    if (mipmaps)
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

}

vector<uint8_t> Texture2DArray::GetData(int32_t depth) {

    auto framebuffer = Framebuffer(width, height);

    vector<uint8_t> data = vector<uint8_t>(width * height * channels);

    framebuffer.AddComponentTextureLayer(GL_COLOR_ATTACHMENT0, this);

    glReadPixels(0, 0, width, height, GetBaseFormat(sizedFormat), GL_UNSIGNED_BYTE, data.data());

    framebuffer.DeleteContent();

    framebuffer.Unbind();

    return data;

}

void Texture2DArray::Resize(int32_t width, int32_t height, int32_t depth) {



}

void Texture2DArray::SaveToPNG(string filename, int32_t depth) {



}

void Texture2DArray::ReserveStorage(int32_t mipCount) {



}