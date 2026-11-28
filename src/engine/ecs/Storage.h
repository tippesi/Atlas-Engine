#pragma once

#include "../System.h"
#include "Entity.h"

#include <vector>

namespace Atlas {

    namespace ECS {

        class Storage {

            using Page = std::vector<uint32_t>;

        public:
            Storage() {}

            void Emplace(const Entity entity);

            virtual void Erase(const Entity entity);

            inline Entity& operator[](size_t idx) {

                return packedData[idx];

            }

            inline const Entity& operator[](size_t idx)  const {

                return packedData[idx];

            }

            inline bool Contains(const Entity entity) const {

                auto idx = EntityToIdx(entity);
                auto page = GetPage(idx);

                return page < pageTableSize && pageData[page].size() &&
                    pageData[page][GetOffset(idx)] != EntityConfig::InvalidEntity;

            }

            inline size_t Size()  const {

                return packedData.size();

            }

            const uint32_t pageSize = 4096;
            const uint32_t pageSizePowOf2 = 12;

        protected:
            inline size_t GetPage(uint32_t idx) const {

                return size_t(idx >> pageSizePowOf2);

            }

            inline size_t GetOffset(uint32_t idx)  const {

                return size_t(idx & (pageSize - 1));

            }

            inline size_t GetIndex(const Entity entity) const {

                auto idx = EntityToIdx(entity);
                return pageData[GetPage(idx)][GetOffset(idx)];

            }

            inline size_t TryGetIndex(const Entity entity) const {

                auto idx = EntityToIdx(entity);
                auto page = GetPage(idx);
                auto offset = GetOffset(idx);

                if (page >= pageData.size() || offset >= pageData[page].size())
                    return EntityConfig::InvalidEntity;

                return pageData[page][offset];

            }

        private:
            std::vector<Page> pageData;
            std::vector<Entity> packedData;

            size_t pageTableSize = 0;

        };

    }

}
