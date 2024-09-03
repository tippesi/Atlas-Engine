#pragma once

#include <atomic>

namespace Atlas {

	class JobSemaphore {

	public:
		inline void Reset() { counter.store(0); }

		inline void Release() { counter.fetch_add(1); }

		inline bool TryAquire() { return counter.load() > 0; }

	private:
		std::atomic_int counter = 0;

	};

}