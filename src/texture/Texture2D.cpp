#include "Texture2D.h"

Texture2D::Texture2D(GLenum dataType, int32_t width, int32_t height, int32_t sizedFormat, int32_t wrapping,
        int32_t filtering, bool anisotropicFiltering, bool generateMipMaps) : width(width), height(height) {

    Generate(GL_TEXTURE2D, dataType, sizedFormat, wrapping, filtering, anisotropicFiltering, generateMipMaps);

}