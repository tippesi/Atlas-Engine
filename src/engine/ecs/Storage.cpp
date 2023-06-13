#include "Storage.h"

namespace Atlas {

    namespace ECS {

        void Storage::Emplace(const Entity entity) {

            auto idx = EntityToIdx(entity);
            auto pageIdx = GetPage(idx);
            auto offsetIdx = GetOffset(idx);

            if (pageData.size() <= pageIdx) {
                pageTableSize = pageIdx + 1;
                pageData.resize(pageTableSize);
            }

            auto& page = pageData[pageIdx];

            if (!page.size()) {
                page.resize(size_t(pageSize));
                std::fill(page.begin(), page.end(), uint32_t(EntityConfig::InvalidEntity));
            }

            page[offsetIdx] = uint32_t(packedData.size());
            packedData.push_back(entity);

        }

        void Storage::Erase(const Entity entity) {

            auto idx = EntityToIdx(entity);
            auto pageIdx = GetPage(idx);
            auto offsetIdx = GetOffset(idx);

            // Get the last element and use it to replace the
            // to be removed element. Update page table.
            auto back = packedData.back();
            idx = EntityToIdx(back);

            auto pos = pageData[pageIdx][offsetIdx];
            pageData[GetPage(idx)][GetOffset(idx)] = pos;
            packedData[size_t(pos)] = Entity(back);

            // Don't forget to invalidate the data
            pageData[pageIdx][offsetIdx] = EntityConfig::InvalidEntity;
            packedData.pop_back();

        }

    }

}