#pragma once

#include "Pool.h"
#include "TypeIndex.h"

#include <vector>
#include <algorithm>
#include <memory>

namespace Atlas {

    namespace ECS {

        class Pools {

        public:
            Pools() = default;

            template<typename Comp>
            Pool<Comp>& Get();

        private:
            struct PoolData {
                uint64_t idx;
                std::shared_ptr<Storage> storage;
            };

            std::vector<PoolData> data;

            friend class EntityManager;

        };

        template<typename Comp>
        Pool<Comp>& Pools::Get() {
            Storage* storage = nullptr;

            auto idx = TypeIndex::Get<Comp>();
            auto find = std::find_if(data.begin(), data.end(), [idx](const auto& poolData) { return idx == poolData.idx; });

            if (find != data.end()) {

                storage = find->storage.get();

            }
            else {

                // https://stackoverflow.com/questions/15783342/should-i-use-c11-emplace-back-with-pointers-containters
                // Need shared_ptr here such that destructor of Pool is called, not destructor of Storage
                data.emplace_back(PoolData{ idx, std::make_shared<Pool<Comp>>() });
                storage = data.back().storage.get();

            }

            return *static_cast<Pool<Comp>*>(storage);

        }

    }

}