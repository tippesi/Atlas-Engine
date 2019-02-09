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

#endif

