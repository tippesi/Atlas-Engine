#ifndef AE_ECSTYPEINDEX_H
#define AE_ECSTYPEINDEX_H

#include <cstdint>

namespace Atlas {

    namespace ECS {

        class TypeIndex {

        public:
            template<typename Type>
            static const uint64_t Get() noexcept {

                static const uint64_t value = Identifier();
                return value;

            }

            template<typename S, typename T>
            static const bool Equal() noexcept {

                static const bool equal = (TypeIndex::Get<S>() == TypeIndex::Get<T>());
                return equal;

            }

        private:
            static uint64_t Identifier() noexcept {

                static uint64_t value = 0;
                return value++;

            }

        };

    }

}

#endif