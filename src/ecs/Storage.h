#ifndef AE_ECSSTORAGE_H
#define AE_ECSSTORAGE_H

#include "Entity.h"

#include <vector>

namespace Atlas {

	namespace ECS {

		class Storage {

			using Page = std::vector<uint32_t>;

		public:
			Storage() = default;

			Entity& operator[](size_t idx);

			const Entity& operator[](size_t idx) const;

			void Emplace(const Entity entity);

			void Erase(const Entity entity);

			bool Contains(const Entity entity) const;

			size_t Size() const;

			const uint32_t pageSize = 4096;
			const uint32_t pageSizePowOf2 = 12;

		protected:
			inline size_t GetPage(uint32_t idx) const;

			inline size_t GetOffset(uint32_t idx) const;

			size_t GetIndex(const Entity entity) const;

		private:
			std::vector<Page> pageData;
			std::vector<Entity> packedData;

			size_t pageTableSize = 0;

		};

	}

}

#endif