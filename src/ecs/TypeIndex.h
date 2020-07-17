#ifndef AE_ECSTYPEINDEX_H
#define AE_ECSTYPEINDEX_H

#include <cstdint>

namespace Atlas {

	namespace ECS {

		class TypeIndex {

		public:
			template<typename Type>
			static uint64_t Get() noexcept {

				static const uint64_t value = Identifier();
				return value;

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