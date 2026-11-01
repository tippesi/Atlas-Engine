#pragma once

#include "Profiler.h"
#include "../graphics/Common.h"
#include "../graphics/CommandList.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#define GPU_PROFILER_MAX_THREADS 32

namespace Atlas {

    /**
     * Measures command lists with timestamp queries. The results of a frame are read back FRAME_DATA_COUNT frames later.
     */
    class GPUProfiler : public Profiler {

    public:
        using Scope = BasicScope<GPUProfiler, const std::string&>;

        /**
         * Begins a new frame. Is called externally by the engine.
         */
        static void BeginFrame();

        /**
         * Shuts down the profiler, destroys resources. Is called externally be the engine
         */
        static void Shutdown();

        /**
         * Begin recording a thread.
         * @param name
         * @param commandList Optional parameter to pass a command list.
         * @note A command list will be needed before recording BeginQuery() calls
         */
        static void BeginThread(const std::string& name, Graphics::CommandList* commandList = nullptr);

        /**
         * Ends the recording of a thread
         */
        static void EndThread();

        /**
         * Sets the command list for the current thread
         * @param commandList The command list
         * @note A command list will be needed before recording BeginQuery() calls
         */
        static void SetCommandList(Graphics::CommandList* commandList);

        /**
         * Begin a query.
         * @param name The name of the query
         * @return False if the profiler is disabled, then EndQuery() must not be called
         */
        static bool BeginQuery(const std::string& name);

        /**
         * End a query.
         * @note There need to be as many end queries as
         * there were begin queries.
         */
        static void EndQuery();

        /**
         * Ends a query and begins a new one
         * @param name The name of the new query
         * @note This internally just calls EndQuery() and BeginQuery()
         * afterwards to improve usability
         */
        static void EndAndBeginQuery(const std::string& name);

        /**
         * Gets the queries of the last frame that was read back
         * @param order The order of the query. Chronologically be default.
         */
        static std::vector<ThreadData> GetQueries(OrderBy order = OrderBy::CHRONO);

        /**
         * Gets the averaged queries of the last n frames
         * @param frameCount The amount of frames to use in the average
         * @param order The order of the query. Chronologically be default.
         */
        static std::vector<ThreadData> GetQueriesAverage(uint32_t frameCount = 32, OrderBy order = OrderBy::CHRONO);

        static std::atomic_bool enable;

    private:
        // A query while recording, the timestamps are read back later
        struct TimestampQuery {
            std::string name;

            uint32_t startId = 0;
            uint32_t endId = 0;

            std::vector<TimestampQuery> children;
        };

        struct ThreadContext {
            std::thread::id id;

            std::string name;
            Graphics::CommandList* commandList = nullptr;

            std::vector<TimestampQuery> stack;
            std::vector<TimestampQuery> queries;

            Ref<Graphics::QueryPool> queryPool;
            uint32_t poolIdx = 0;

            size_t frameIdx = 0;
            bool isValid = false;
        };

        struct ThreadHistory {
            size_t historyIdx = 0;
            std::vector<std::vector<Query>> history;
        };

        static constexpr uint32_t historyLength = 64;

        static Query EvaluateQuery(const TimestampQuery& query, const std::vector<uint64_t>& timeData);

        static std::vector<ThreadData> GetThreadData(uint32_t frameCount, OrderBy order);

        static ThreadContext& GetThreadContext();

        static std::atomic_int threadContextCount;
        static std::vector<ThreadContext> threadContexts;

        static std::mutex unevaluatedThreadContextsMutex;
        static std::deque<ThreadContext> unevaluatedThreadContexts;

        static std::unordered_map<std::string, ThreadHistory> queryHistory;
        static size_t frameIdx;

        static bool supportsDebugMarkers;

    };

}
