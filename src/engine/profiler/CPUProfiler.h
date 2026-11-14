#pragma once

#include "Profiler.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#if defined(_M_X64) || defined(__x86_64__)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#define AE_CPU_PROFILER_TSC
#elif defined(_M_ARM64) || defined(__aarch64__)
#ifdef _MSC_VER
#include <intrin.h>
#endif
#define AE_CPU_PROFILER_ARM_COUNTER
#endif

namespace Atlas {

    /**
     * Measures scopes on all threads. A query reads the time stamp counter twice and writes one entry into a buffer
     * of its thread, without locks or allocations. BeginFrame() collects the entries of all threads once per frame.
     * @note Query names aren't copied, they must stay valid as long as the profiler runs (string literals)
     */
    class CPUProfiler : public Profiler {

    public:
        using Scope = BasicScope<CPUProfiler, const char*>;

        /**
         * Collects the queries of all threads that ended since the last frame. Is called by the engine.
         */
        static void BeginFrame();

        /**
         * Names the current thread in the results, otherwise it is numbered
         */
        static void SetThreadName(const std::string& name);

        /**
         * Begins a query on the current thread.
         * @return False if the profiler is disabled, then EndQuery() must not be called
         */
        static inline bool BeginQuery(const char* name) {

            if (!enable.load(std::memory_order_relaxed))
                return false;

            auto buffer = threadBuffer;
            if (!buffer) {
                buffer = RegisterThread();
                if (!buffer)
                    return false;
            }

            auto depth = buffer->depth++;
            if (depth < maxDepth) {
                buffer->stack[depth].name = name;
                buffer->stack[depth].start = Now();
            }

            return true;

        }

        /**
         * Ends the last query of the current thread
         */
        static inline void EndQuery() {

            auto end = Now();
            auto buffer = threadBuffer;
            if (!buffer || !buffer->depth)
                return;

            auto depth = --buffer->depth;
            if (depth >= maxDepth)
                return;

            auto index = buffer->written.load(std::memory_order_relaxed);
            buffer->events[index & (eventCapacity - 1)] = { buffer->stack[depth].name, buffer->stack[depth].start, end, depth };
            buffer->written.store(index + 1, std::memory_order_release);

        }

        /**
         * Gets the queries of the last frame
         */
        static std::vector<ThreadData> GetQueries(OrderBy order = OrderBy::CHRONO);

        /**
         * Gets the queries averaged over the last frames, the calls are averaged as well
         */
        static std::vector<ThreadData> GetQueriesAverage(uint32_t frameCount = 32, OrderBy order = OrderBy::CHRONO);

        static std::atomic_bool enable;

    private:
        struct Event {
            const char* name;
            uint64_t start;
            uint64_t end;
            uint32_t depth;
        };

        // A query while collecting, the names are converted when the results are requested
        struct Node {
            const char* name = nullptr;
            uint64_t startTime = 0;
            uint64_t elapsedTime = 0;
            uint32_t calls = 0;
            std::vector<Node> children;
        };

        struct StackEntry {
            const char* name;
            uint64_t start;
        };

        static constexpr uint32_t eventCapacity = 8192;
        static constexpr uint32_t maxDepth = 64;
        static constexpr uint32_t historyLength = 64;

        struct ThreadBuffer {
            // Written by the thread itself
            uint32_t depth = 0;
            StackEntry stack[maxDepth];
            std::vector<Event> events;
            std::atomic_uint64_t written = 0;
            std::atomic_bool released = false;

            // Used while collecting
            std::string name;
            uint32_t order = 0;
            uint64_t read = 0;
            // Finished queries of each depth waiting for their parent
            std::vector<std::vector<Node>> pending;
        };

        struct ThreadHistory {
            uint32_t order = 0;
            uint32_t index = 0;
            uint64_t lastFrame = 0;
            std::vector<std::vector<Node>> frames;
        };

        struct ThreadRelease;

        static inline uint64_t Now() {

#if defined(AE_CPU_PROFILER_TSC)
            return __rdtsc();
#elif defined(AE_CPU_PROFILER_ARM_COUNTER) && defined(_MSC_VER)
            // The virtual counter of the generic timer, CNTVCT_EL0
            return uint64_t(_ReadStatusReg(ARM64_SYSREG(3, 3, 14, 0, 2)));
#elif defined(AE_CPU_PROFILER_ARM_COUNTER)
            uint64_t ticks;
            asm volatile("mrs %0, cntvct_el0" : "=r"(ticks));
            return ticks;
#else
            return uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
#endif

        }

        static ThreadBuffer* RegisterThread();

        static void Collect(ThreadBuffer& buffer);

        static Query ToQuery(const Node& node);

        static std::vector<ThreadData> GetThreadData(uint32_t frameCount, OrderBy order);

        static inline thread_local ThreadBuffer* threadBuffer = nullptr;

        static std::mutex threadsMutex;
        static std::vector<std::unique_ptr<ThreadBuffer>> threads;
        static uint32_t threadCount;

        static uint64_t startTicks;
        static std::chrono::steady_clock::time_point startTime;
        static double nanosecondsPerTick;

        static std::mutex historyMutex;
        static std::unordered_map<std::string, ThreadHistory> history;
        static uint64_t frameIdx;

    };

}
