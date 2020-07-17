#include "Storage.h"

namespace Atlas {

	namespace ECS {

		Entity& Storage::operator[](size_t idx) {

			return packedData[idx];

		}

		const Entity& Storage::operator[](size_t idx) const {

			return packedData[idx];

		}

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