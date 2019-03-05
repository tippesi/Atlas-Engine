#ifndef AE_TYPEFORMAT_H
#define AE_TYPEFORMAT_H

// This file defines type formats independently from
// the graphics API that is used.

#include "System.h"

#if defined(AE_API_GLES) || defined(AE_API_GL)
#define AE_UBYTE GL_UNSIGNED_BYTE
#define AE_BYTE GL_BYTE
#define AE_USHORT GL_UNSIGNED_SHORT
#define AE_SHORT GL_SHORT
#define AE_UINT GL_UNSIGNED_INT
#define AE_INT GL_INT
#define AE_HALF_FLOAT GL_HALF_FLOAT
#define AE_FLOAT GL_FLOAT
#define AE_INT_2_10_10_10 GL_INT_2_10_10_10_REV
#define AE_UINT_2_10_10_10 GL_UNSIGNED_INT_2_10_10_10_REV
#endif

namespace Atlas {

    class TypeFormat {

    public:
        /**
         * Determines the size of the type
         * @param typeFormat The format which should be checked on size
         * @return The size of the format in bytes.
         */
        static int32_t GetSize(uint32_t typeFormat) {
            switch(typeFormat) {
                case AE_UBYTE: return 1;
                case AE_BYTE: return 1;
                case AE_USHORT: return 2;
                case AE_SHORT: return 2;
                case AE_UINT: return 4;
                case AE_INT: return 4;
                case AE_HALF_FLOAT: return 2;
                case AE_FLOAT: return 4;
                case AE_INT_2_10_10_10: return 4;
                case AE_UINT_2_10_10_10: return 4;
				default: return 4;
            }
        }

    };

}

#endif