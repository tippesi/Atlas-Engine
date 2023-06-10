#ifndef AE_ECSPOOLS_H
#define AE_ECSPOOLS_H

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
                std::unique_ptr<Storage> storage;
            };

            std::vector<PoolData> data;

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
                data.emplace_back(PoolData{ idx, std::make_unique<Pool<Comp>>() });
                storage = data.back().storage.get();

            }

            return *static_cast<Pool<Comp>*>(storage);

        }

    }

}

#endif