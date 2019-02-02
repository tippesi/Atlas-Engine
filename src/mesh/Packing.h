#ifndef AE_PACKING_H
#define AE_PACKING_H

#include "../System.h"

/// <summary>
/// Packs a vector into a 32 bit integer
/// </summary>
/// <param name="vector">The vector to pack into the integer.</param>
/// <return>An unsigned integer including the vector.</return>
uint32_t packNormalizedFloat_2_10_10_10_REV(vec4 vector);

/// <summary>
/// Unpacks a vector from a 32 bit integer
/// </summary>
/// <param name="packed">The integer where the vector is packed in.</param>
/// <return>A 4-component vector including the unpacked data.</return>
vec4 unpackNormalizedFloat_2_10_10_10_REV(uint32_t packed);

#endif