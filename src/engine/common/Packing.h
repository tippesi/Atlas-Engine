#ifndef AE_PACKING_H
#define AE_PACKING_H

#include "../System.h"

namespace Atlas {

    namespace Common {

        namespace Packing {

            /**
             * Packs a vector into a 32 bit integer
             * @param vector The vector to pack into the integer.
             * @return An unsigned integer including the vector.
             * @note This packing function is needed for OpenGL compatible packing.
             */
            uint32_t PackNormalizedFloat3x10_1x2(vec4 vector);

            /**
             * Unpacks a vector from a 32 bit integer
             * @param packed The integer where the vector is packed in.
             * @return A 4-component vector including the unpacked data.
             * @note This packing function is needed for OpenGL compatible packing.
             */
            vec4 UnpackNormalizedFloat3x10_1x2(uint32_t packed);

            /**
             * Packs a vector into a 32 bit integer
             * @param vector The vector to pack into the integer.
             * @return An unsigned integer including the vector.
             * @note The vectors components should be in range [-1:1]
             */
            int32_t PackSignedVector3x10_1x2(vec4 vector);

            /**
             * Unpacks a vector from a 32 bit integer
             * @param packed The integer where the vector is packed in.
             * @return A 4-component vector including the unpacked data.
             * @note The vectors components will be in range [-1:1]
             */
            vec4 UnpackSignedVector3x10_1x2(int32_t packed);

            /**
             * Packs a vector into a 32 bit integer
             * @param vector The vector to pack into the integer.
             * @return An unsigned integer including the vector.
             * @note The vectors components should be in range [0:1]
             */
            int32_t PackUnsignedVector3x10_1x2(vec4 vector);

            /**
             * Unpacks a vector from a 32 bit integer
             * @param packed The integer where the vector is packed in.
             * @return A 4-component vector including the unpacked data.
             * @note The vectors components will be in range [0:1]
             */
            vec4 UnpackUnsignedVector3x10_1x2(int32_t packed);

        }

    }

}

#endif