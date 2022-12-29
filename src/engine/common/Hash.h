#ifndef AE_HASH_H
#define AE_HASH_H

#include <functional>
#include <cstdint>

namespace Atlas {

    template <class T>
    inline void HashCombine(size_t& hash, const T& v) {
        std::hash<T> hasher;
        hash ^= hasher(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }

}

#endif