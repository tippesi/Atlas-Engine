#ifndef AE_PACKING_H
#define AE_PACKING_H

#include "../System.h"

namespace Atlas {

    namespace Mesh {

        /**
         * Packs a vector into a 32 bit integer
         * @param vector The vector to pack into the integer.
         * @return An unsigned integer including the vector.
         */
        uint32_t packNormalizedFloat_2_10_10_10_REV(vec4 vector);

        /**
         * Unpacks a vector from a 32 bit integer
         * @param packed The integer where the vector is packed in.
         * @return A 4-component vector including the unpacked data.
         */
        vec4 unpackNormalizedFloat_2_10_10_10_REV(uint32_t packed);


    }

}

#endif