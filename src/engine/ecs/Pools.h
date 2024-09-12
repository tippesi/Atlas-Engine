#pragma once

#include "Pool.h"
#include "TypeIndex.h"

#include <array>
#include <algorithm>
#include <memory>

namespace Atlas {

    namespace ECS {

        class Pools {

        public:
            Pools() = default;

            template<typename Comp>
            Pool<Comp>& Get();
            
            void Clear() {
            
                for (auto& poolData : data) {
                    poolData.storage.reset();
                }

            }

        private:
            struct PoolData {
                std::shared_ptr<Storage> storage = nullptr;
            };
            
            static constexpr uint64_t dataCount = 1024;

            std::array<PoolData, dataCount> data;

            friend class EntityManager;

        };

        template<typename Comp>
        Pool<Comp>& Pools::Get() {
            Storage* storage = nullptr;

            auto idx = TypeIndex::Get<Comp>();
            
            AE_ASSERT(idx < dataCount && "Too many component types stored. Please increase pool capacity");
            auto& poolData = data[idx];

            if (poolData.storage != nullptr) {

                storage = poolData.storage.get();

            }
            else {

                // https://stackoverflow.com/questions/15783342/should-i-use-c11-emplace-back-with-pointers-containters
                // Need shared_ptr here such that destructor of Pool is called, not destructor of Storage
                poolData.storage = std::make_shared<Pool<Comp>>();
                storage = poolData.storage.get();

            }

            return *static_cast<Pool<Comp>*>(storage);

        }

    }

}
