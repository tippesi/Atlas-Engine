#ifndef AE_GRAPHICSPROFILER_H
#define AE_GRAPHICSPROFILER_H

#include "Common.h"
#include "CommandList.h"

#include <unordered_map>
#include <vector>
#include <atomic>
#include <thread>
#include <deque>

#define PROFILER_MAX_THREADS 32

namespace Atlas {

    namespace Graphics {

        class Profiler {

        public:
            /**
             * Measures the elapsed time between two points
             */
            struct Timer {
                int32_t poolId = 0;
                int32_t startId = 0;
                int32_t endId = 0;

                uint64_t startTime = 0;
                uint64_t endTime = 0;

                uint64_t elapsedTime = 0;
            };

            /**
             * Represents a query with a list of subqueries
             * The name of the query needs to be unique at the
             * given query tree depth.
             */
            struct Query {
                std::string name;

                Timer timer;
                std::vector<Query> children;

                size_t stackLevel = 0;
            };

            struct ThreadHistory {
                std::string threadName;
                size_t historyIdx = 0;
                std::vector<std::vector<Profiler::Query>> history;
            };

            struct ThreadData {
                std::string name;
                std::vector<Profiler::Query> queries;
            };

            /**
             * Orders retrieved queries.
             * CHRONO: The queries have the order in which they were created
             * MAX_TIME: Order queries by their elapsed time in descending order
             * MIN_TIME: Order queries by their elapsed time in ascending order
             */
            enum class OrderBy {
                CHRONO,
                MAX_TIME,
                MIN_TIME
            };

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
            static void BeginThread(const std::string& name, CommandList* commandList = nullptr);

            /**
             * Ends the recording of a thread
             */
            static void EndThread();

            /**
             * Sets the command list for the current thread
             * @param commandList The command list
             * @note A command list will be needed before recording BeginQuery() calls
             */
            static void SetCommandList(CommandList* commandList);

            /**
             * Begin a query.
             * @param name The name of the query
             * @note The query name needs to be unique at the current
             * level of the query tree
             */
            static void BeginQuery(const std::string& name);

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
             * Gets the queries of the last frame
             * @param order The order of the query. Chronologically be default.
             * @return
             * @note It is not possible to retrieve the queries
             * of the current frame without stalling the pipeline. This
             * method therefore return the queries of the last frame
             */
            static std::vector<Profiler::ThreadData> GetQueries(OrderBy order = OrderBy::CHRONO);

            /**
             * Gets the averaged queries of the last n frames
             * @param frameCount The amount of frames to use in the average
             * @param order The order of the query. Chronologically be default.
             * @return
             */
            static std::vector<Profiler::ThreadData> GetQueriesAverage(uint32_t frameCount = 32,
                OrderBy order = OrderBy::CHRONO);

        private:
            struct ThreadContext {
                std::thread::id id;

                std::string name;
                CommandList* commandList;

                std::vector<Query> stack;
                std::vector<Query> queries;

                std::vector<Ref<QueryPool>> queryPools;
                uint32_t poolIdx = 0;

                size_t frameIdx;
                bool isValid = false;
            };

            static void EvaluateQuery(ThreadContext& context, Query& query);

            static void OrderQueries(std::vector<Query>& queries, OrderBy order);

            static void UpdateHistory();

            static void EvaluateHistory(ThreadHistory& history);

            static std::vector<Query> AddQueriesToAverage(std::vector<Query> average,
                std::vector<Query> queries,	uint64_t frameIdx, uint64_t frameCount);

            static ThreadContext& GetThreadContext();

            static std::atomic_int threadContextCount;
            static std::vector<ThreadContext> threadContexts;

            static std::mutex unevaluatedThreadContextsMutex;
            static std::deque<ThreadContext> unevaluatedThreadContexts;

            static std::unordered_map<std::string, ThreadHistory> queryHistory;
            static size_t frameIdx;

            static bool activate;

        };

    }

}

#endif
