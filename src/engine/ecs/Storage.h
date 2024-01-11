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

            void Erase(const Entity entity);

            inline Entity& operator[](size_t idx);

            inline const Entity& operator[](size_t idx) const;

            inline bool Contains(const Entity entity) const;

            inline size_t Size() const;

            const uint32_t pageSize = 4096;
            const uint32_t pageSizePowOf2 = 12;

        protected:
            inline size_t GetPage(uint32_t idx) const;

            inline size_t GetOffset(uint32_t idx) const;

            inline size_t GetIndex(const Entity entity) const;

        private:
            std::vector<Page> pageData;
            std::vector<Entity> packedData;

            size_t pageTableSize = 0;

        };

        Entity& Storage::operator[](size_t idx) {

            return packedData[idx];

        }

        const Entity& Storage::operator[](size_t idx) const {

            return packedData[idx];

        }

        bool Storage::Contains(const Entity entity) const {

            auto idx = EntityToIdx(entity);
            auto page = GetPage(idx);

            return page < pageTableSize&& pageData[page].size() &&
                pageData[page][GetOffset(idx)] != EntityConfig::InvalidEntity;

        }

        size_t Storage::Size() const {

            return packedData.size();

        }

        size_t Storage::GetPage(uint32_t idx) const {

            return size_t(idx >> pageSizePowOf2);

        }

        size_t Storage::GetOffset(uint32_t idx) const {

            return size_t(idx & (pageSize - 1));

        }

        size_t Storage::GetIndex(const Entity entity) const {

            auto idx = EntityToIdx(entity);
            return pageData[GetPage(idx)][GetOffset(idx)];

        }

    }

}