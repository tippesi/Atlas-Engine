#pragma once

#include <semaphore>
#include <atomic>

namespace Atlas {

    // This whole construct is here to avoid undefined behaviour of the semaphore when released more than once before one acquire
    class Signal {

    public:
        inline void Notify() {

            bool expected = false;
            if (condition.compare_exchange_strong(expected, true)) {
                semaphore.release();
            }

        }

        inline void Wait() {

            semaphore.acquire();
            condition = false;

        }

    private:
        std::atomic_bool condition {false};
        std::binary_semaphore semaphore {0};

    };

}