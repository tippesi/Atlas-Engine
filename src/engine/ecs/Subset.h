#pragma once

#include "Pool.h"

#include <memory>
#include <tuple>
#include <limits>
#include <array>
#include <algorithm>

namespace Atlas {

    namespace ECS {

        /**
         * A subsets is a iterable class to calculate a subset of entities
         * from the whole entity manager depending on their components
         * @tparam Comp The component types
         */
        template<typename... Comp>
        class Subset {

            using OtherStorage = std::array<const Storage*, (sizeof...(Comp) - 1)>;

        public:
            class Iterator {

            public:
                Iterator() = delete;

                Iterator(const Storage* mainStorage, OtherStorage otherStorages, size_t idx) :
                    mainStorage(mainStorage), otherStorages(otherStorages), idx(idx) {

                    if (idx < mainStorage->Size() && !HasAllComponents()) {
                        operator++();
                    }

                }

                Iterator& operator++() {

                    while (++idx < mainStorage->Size() && !HasAllComponents());
                    return *this;

                }

                Iterator& operator--() {

                    while (--idx > 0 && !HasAllComponents());
                    return *this;

                }

                inline bool operator!=(const Iterator& iterator) const {

                    return idx != iterator.idx;

                }

                inline const Entity& operator*() const {

                    return entity;

                }

            protected:
                inline bool HasAllComponents() {

                    entity = (*mainStorage)[idx];
                    bool valid = true;

                    for (auto& storage : otherStorages) {
                        valid &= storage->Contains(entity);
                    }

                    return valid;

                }

                size_t idx = 0;

                Entity entity;
                const Storage* mainStorage = nullptr;
                OtherStorage otherStorages;

            };

            Subset() = default;

            Subset(std::tuple<Pool<Comp>*...> pools) : pools(pools) {

                // Find the shortest pool
                auto max = std::numeric_limits<size_t>::max();

                const auto findMin = [&](Storage* storage) {
                    if (storage->Size() < max) {
                        mainStorage = static_cast<const Storage*>(storage);
                        max = storage->Size();
                    }
                };

                std::apply([&](auto ...args) {(..., findMin(args)); }, pools);

                // Find all the other pools
                size_t count = 0;

                const auto findOther = [&](Storage* storage) {
                    if (storage != mainStorage)
                        otherStorages[count++] = static_cast<const Storage*>(storage);
                };

                std::apply([&](auto ...args) {(..., findOther(args)); }, pools);

            }

            template<typename... Component>
            decltype(auto) Get(const Entity entity) {

                if constexpr (sizeof...(Component) == 0) {
                    if constexpr (sizeof...(Comp) == 1) {
                        return (std::get<Pool<Comp>*>(pools)->Get(entity), ...);
                    }
                    else {
                        return std::tuple_cat(std::forward_as_tuple(std::get<Pool<Comp>*>(pools)->Get(entity))...);
                    }
                }
                else {
                    if constexpr (sizeof...(Component) == 1) {
                        return (std::get<Pool<Component>*>(pools)->Get(entity), ...);
                    }
                    else {
                        return std::tuple_cat(std::forward_as_tuple(std::get<Pool<Component>*>(pools)->Get(entity))...);
                    }
                }

            }

            Iterator begin() const {

                return { mainStorage, otherStorages, 0 };

            }

            Iterator end() const {

                return { mainStorage, otherStorages, mainStorage->Size() };

            }

            bool Any() const {

                size_t idx = 0;
                while (++idx < mainStorage->Size())
                    if (HasAllComponents(idx))
                        return true;

                return false;

            }

        public:
            inline bool HasAllComponents(size_t idx) const {

                auto entity = (*mainStorage)[idx];
                bool valid = true;

                for (auto& storage : otherStorages) {
                    valid &= storage->Contains(entity);
                }

                return valid;

            }

            std::tuple<Pool<Comp>*...> pools;

            const Storage* mainStorage = nullptr;
            OtherStorage otherStorages;

        };


    }

}