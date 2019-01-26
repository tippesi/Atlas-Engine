#ifndef TEXTUREFORMAT_H
#define TEXTUREFORMAT_H

// This file defines texture formats independently from
// the graphics API that is used.

#include "../System.h"

// Unsized texture formats
#if defined(ENGINE_GLES) || defined(ENGINE_GL)
#define R GL_RED
#define RG GL_RG
#define RGB GL_RGB
#define RGBA GL_RGBA
#define R_INT GL_RED_INTEGER
#define RG_INT GL_RG_INTEGER
#define RGB_INT GL_RGB_INTEGER
#define RGBA_INT GL_RGBA_INTEGER
#define DEPTH GL_DEPTH_COMPONENT
#endif

// Sized texture formats
#if defined(ENGINE_GLES) || defined(ENGINE_GL)
#define R8 GL_R8
#define R8I GL_R8I
#define R8UI GL_R8UI
#define R16F GL_R16F
#define R16I GL_R16I
#define R16UI GL_R16UI
#define R32F GL_R32F
#define R32I GL_R32I
#define R32UI GL_R32UI
#define RG8 GL_RG8
#define RG8I GL_RG8I
#define RG8UI GL_RG8UI
#define RG16F GL_RG16F
#define RG16I GL_RG16I
#define RG16UI GL_RG16UI
#define RG32F GL_RG32F
#define RG32I GL_RG32I
#define RG32UI GL_RG32UI
#define RGB8 GL_RGB8
#define RGB8I GL_RGB8I
#define RGB8UI GL_RGB8UI
#define RGB16F GL_RGB16F
#define RGB16I GL_RGB16I
#define RGB16UI GL_RGB16UI
#define RGB32F GL_RGB32F
#define RGB32I GL_RGB32I
#define RGB32UI GL_RGB32UI
#define SRGB8 GL_SRGB8
#define RGBA8 GL_RGBA8
#define RGBA8I GL_RGBA8I
#define RGBA8UI GL_RGBA8UI
#define RGBA16F GL_RGBA16F
#define RGBA16I GL_RGBA16I
#define RGBA16UI GL_RGBA16UI
#define RGBA32F GL_RGBA32F
#define RGBA32I GL_RGBA32I
#define RGBA32UI GL_RGBA32UI
#define SRGBA8 GL_SRGB8_ALPHA8
#define DEPTH16 GL_DEPTH_COMPONENT16
#define DEPTH24 GL_DEPTH_COMPONENT24
#define DEPTH32F GL_DEPTH_COMPONENT32F
#endif

class TextureFormat {

public:

    /**
     * Determines the base format of a sized format.
     * @param sizedFormat The sized format, e.g GL_RGB16F
     * @return The base format as an integer, e.g GL_RGB
     */
    static int32_t GetBaseFormat(int32_t sizedFormat) {
        switch (sizedFormat) {
            case R8: return R;
            case R8I: return R_INT;
            case R8UI: return R_INT;
            case R16F: return R;
            case R16I: return R_INT;
            case R16UI: return R_INT;
            case R32F: return R;
            case R32I: return R_INT;
            case R32UI: return R_INT;
            case RG8: return RG;
            case RG8I: return RG_INT;
            case RG8UI: return RG_INT;
            case RG16F: return RG;
            case RG16I: return RG_INT;
            case RG16UI: return RG_INT;
            case RG32F: return RG;
            case RG32I: return RG_INT;
            case RG32UI: return RG_INT;
            case RGB8: return RGB;
            case RGB8I: return RGB_INT;
            case RGB8UI: return RGB_INT;
            case RGB16F: return RGB;
            case RGB16I: return RGB_INT;
            case RGB16UI: return RGB_INT;
            case RGB32F: return RGB;
            case RGB32I: return RGB_INT;
            case RGB32UI: return RGB_INT;
            case SRGB8: return RGB;
            case RGBA8: return RGBA;
            case RGBA8I: return RGBA_INT;
            case RGBA8UI: return RGBA_INT;
            case RGBA16F: return RGBA;
            case RGBA16I: return RGBA_INT;
            case RGBA16UI: return RGBA_INT;
            case RGBA32F: return RGBA;
            case RGBA32I: return RGBA_INT;
            case RGBA32UI: return RGBA_INT;
            case SRGBA8: return RGBA;
            case DEPTH16: return DEPTH;
            case DEPTH24: return DEPTH;
            case DEPTH32F: return DEPTH;
            default: return RGB;
        }
    }

};

#endif