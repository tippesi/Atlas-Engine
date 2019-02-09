#ifndef AE_TEXTUREFORMAT_H
#define AE_TEXTUREFORMAT_H

// This file defines texture formats independently from
// the graphics API that is used.

#include "../System.h"
#include "../TypeFormat.h"

// Unsized texture formats
#if defined(AE_API_GLES) || defined(AE_API_GL)
#define AE_R GL_RED
#define AE_RG GL_RG
#define AE_RGB GL_RGB
#define AE_RGBA GL_RGBA
#define AE_R_INT GL_RED_INTEGER
#define AE_RG_INT GL_RG_INTEGER
#define AE_RGB_INT GL_RGB_INTEGER
#define AE_RGBA_INT GL_RGBA_INTEGER
#define AE_DEPTH GL_DEPTH_COMPONENT
#endif

// Sized texture formats
#if defined(AE_API_GLES) || defined(AE_API_GL)
#define AE_R8 GL_R8
#define AE_R8I GL_R8I
#define AE_R8UI GL_R8UI
#define AE_R16F GL_R16F
#define AE_R16I GL_R16I
#define AE_R16UI GL_R16UI
#define AE_R32F GL_R32F
#define AE_R32I GL_R32I
#define AE_R32UI GL_R32UI
#define AE_RG8 GL_RG8
#define AE_RG8I GL_RG8I
#define AE_RG8UI GL_RG8UI
#define AE_RG16F GL_RG16F
#define AE_RG16I GL_RG16I
#define AE_RG16UI GL_RG16UI
#define AE_RG32F GL_RG32F
#define AE_RG32I GL_RG32I
#define AE_RG32UI GL_RG32UI
#define AE_RGB8 GL_RGB8
#define AE_RGB8I GL_RGB8I
#define AE_RGB8UI GL_RGB8UI
#define AE_RGB16F GL_RGB16F
#define AE_RGB16I GL_RGB16I
#define AE_RGB16UI GL_RGB16UI
#define AE_RGB32F GL_RGB32F
#define AE_RGB32I GL_RGB32I
#define AE_RGB32UI GL_RGB32UI
#define AE_SRGB8 GL_SRGB8
#define AE_RGBA8 GL_RGBA8
#define AE_RGBA8I GL_RGBA8I
#define AE_RGBA8UI GL_RGBA8UI
#define AE_RGBA16F GL_RGBA16F
#define AE_RGBA16I GL_RGBA16I
#define AE_RGBA16UI GL_RGBA16UI
#define AE_RGBA32F GL_RGBA32F
#define AE_RGBA32I GL_RGBA32I
#define AE_RGBA32UI GL_RGBA32UI
#define AE_SRGBA8 GL_SRGB8_ALPHA8
#define AE_DEPTH16 GL_DEPTH_COMPONENT16
#define AE_DEPTH24 GL_DEPTH_COMPONENT24
#define AE_DEPTH32F GL_DEPTH_COMPONENT32F
#endif

namespace Atlas {

    namespace Texture {

        class TextureFormat {

        public:

            /**
             * Determines the base format of a sized format.
             * @param sizedFormat The sized format, e.g AE_RGB16F
             * @return The base format as an integer, e.g AE_RGB
             */
            static int32_t GetBaseFormat(int32_t sizedFormat) {
                switch (sizedFormat) {
                    case AE_R8: return AE_R;
                    case AE_R8I: return AE_R_INT;
                    case AE_R8UI: return AE_R_INT;
                    case AE_R16F: return AE_R;
                    case AE_R16I: return AE_R_INT;
                    case AE_R16UI: return AE_R_INT;
                    case AE_R32F: return AE_R;
                    case AE_R32I: return AE_R_INT;
                    case AE_R32UI: return AE_R_INT;
                    case AE_RG8: return AE_RG;
                    case AE_RG8I: return AE_RG_INT;
                    case AE_RG8UI: return AE_RG_INT;
                    case AE_RG16F: return AE_RG;
                    case AE_RG16I: return AE_RG_INT;
                    case AE_RG16UI: return AE_RG_INT;
                    case AE_RG32F: return AE_RG;
                    case AE_RG32I: return AE_RG_INT;
                    case AE_RG32UI: return AE_RG_INT;
                    case AE_RGB8: return AE_RGB;
                    case AE_RGB8I: return AE_RGB_INT;
                    case AE_RGB8UI: return AE_RGB_INT;
                    case AE_RGB16F: return AE_RGB;
                    case AE_RGB16I: return AE_RGB_INT;
                    case AE_RGB16UI: return AE_RGB_INT;
                    case AE_RGB32F: return AE_RGB;
                    case AE_RGB32I: return AE_RGB_INT;
                    case AE_RGB32UI: return AE_RGB_INT;
                    case AE_SRGB8: return AE_RGB;
                    case AE_RGBA8: return AE_RGBA;
                    case AE_RGBA8I: return AE_RGBA_INT;
                    case AE_RGBA8UI: return AE_RGBA_INT;
                    case AE_RGBA16F: return AE_RGBA;
                    case AE_RGBA16I: return AE_RGBA_INT;
                    case AE_RGBA16UI: return AE_RGBA_INT;
                    case AE_RGBA32F: return AE_RGBA;
                    case AE_RGBA32I: return AE_RGBA_INT;
                    case AE_RGBA32UI: return AE_RGBA_INT;
                    case AE_SRGBA8: return AE_RGBA;
                    case AE_DEPTH16: return AE_DEPTH;
                    case AE_DEPTH24: return AE_DEPTH;
                    case AE_DEPTH32F: return AE_DEPTH;
                    default: return AE_RGB;
                }
            }

            /**
             * Determines the type of the sized format
             * @param sizedFormat The sized format, e.g. AE_RGB16F
             * @return The type of the format, e.g. AE_FLOAT
             */
			static int32_t GetType(int32_t sizedFormat) {
                switch (sizedFormat) {
                    case AE_R8: return AE_UBYTE;
                    case AE_R8I: return AE_BYTE;
                    case AE_R8UI: return AE_UBYTE;
                    case AE_R16F: return AE_HALF_FLOAT;
                    case AE_R16I: return AE_SHORT;
                    case AE_R16UI: return AE_USHORT;
                    case AE_R32F: return AE_FLOAT;
                    case AE_R32I: return AE_INT;
                    case AE_R32UI: return AE_UINT;
                    case AE_RG8: return AE_UBYTE;
                    case AE_RG8I: return AE_BYTE;
                    case AE_RG8UI: return AE_UBYTE;
                    case AE_RG16F: return AE_HALF_FLOAT;
                    case AE_RG16I: return AE_SHORT;
                    case AE_RG16UI: return AE_USHORT;
                    case AE_RG32F: return AE_FLOAT;
                    case AE_RG32I: return AE_INT;
                    case AE_RG32UI: return AE_UINT;
                    case AE_RGB8: return AE_UBYTE;
                    case AE_RGB8I: return AE_BYTE;
                    case AE_RGB8UI: return AE_UBYTE;
                    case AE_RGB16F: return AE_HALF_FLOAT;
                    case AE_RGB16I: return AE_SHORT;
                    case AE_RGB16UI: return AE_USHORT;
                    case AE_RGB32F: return AE_FLOAT;
                    case AE_RGB32I: return AE_INT;
                    case AE_RGB32UI: return AE_UINT;
                    case AE_SRGB8: return AE_UBYTE;
                    case AE_RGBA8: return AE_UBYTE;
                    case AE_RGBA8I: return AE_BYTE;
                    case AE_RGBA8UI: return AE_UBYTE;
                    case AE_RGBA16F: return AE_HALF_FLOAT;
                    case AE_RGBA16I: return AE_SHORT;
                    case AE_RGBA16UI: return AE_USHORT;
                    case AE_RGBA32F: return AE_FLOAT;
                    case AE_RGBA32I: return AE_INT;
                    case AE_RGBA32UI: return AE_UINT;
                    case AE_SRGBA8: return AE_UBYTE;
                    case AE_DEPTH16: return AE_USHORT;
                    case AE_DEPTH24: return AE_UINT;
                    case AE_DEPTH32F: return AE_FLOAT;
                    default: return AE_UBYTE;
                }
			}

        };

    }

}

#endif