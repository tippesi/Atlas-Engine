#pragma once

#include <cstdint>
#include <atomic>

namespace Atlas {

    namespace ECS {

        class TypeIndex {

        public:
            template<typename Type>
            static inline uint64_t Get() noexcept {

                static const uint64_t value = Identifier();
                return value;

            }

            template<typename S, typename T>
            static inline bool Equal() noexcept {

                static const bool equal = (TypeIndex::Get<S>() == TypeIndex::Get<T>());
                return equal;

            }

        private:
            static uint64_t Identifier() noexcept {

                static std::atomic<uint64_t> value = 0;
                return value++;

            }

        };

    }

}
