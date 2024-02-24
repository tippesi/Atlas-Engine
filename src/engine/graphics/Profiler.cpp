#include "Profiler.h"
#include "GraphicsDevice.h"

#include <thread>
#include <algorithm>

namespace Atlas {

    namespace Graphics {

        std::atomic_int Profiler::threadContextCount = 0;
        std::vector<Profiler::ThreadContext> Profiler::threadContexts;

        std::mutex Profiler::unevaluatedThreadContextsMutex;
        std::deque<Profiler::ThreadContext> Profiler::unevaluatedThreadContexts;

        std::unordered_map<std::string, Profiler::ThreadHistory> Profiler::queryHistory;
        size_t Profiler::frameIdx = -1;

        bool Profiler::supportsDebugMarkers = false;

#ifndef AE_OS_MACOS
        std::atomic_bool Profiler::enable = true;
#else
        std::atomic_bool Profiler::enable = false;
#endif

        void Profiler::BeginFrame() {

            threadContextCount = 0;
            threadContexts.clear();
            threadContexts.resize(PROFILER_MAX_THREADS);

            if (!enable) return;

            frameIdx++;

            if (!unevaluatedThreadContexts.size()) return;

            // Each frame we replace the query history by a new history
            // This is to make sure old thread names are thrown out the window
            std::unordered_map<std::string, ThreadHistory> newHistory;
            while(unevaluatedThreadContexts.size() && unevaluatedThreadContexts.front().frameIdx + FRAME_DATA_COUNT <= frameIdx) {
                auto& context = unevaluatedThreadContexts.front();
                ThreadHistory history;
                // Check if we can find an old history to fill
                if (queryHistory.contains(context.name)) {
                    history = queryHistory[context.name];
                }
                else {
                    history.history.resize(64);
                }

                std::vector<uint64_t> timeData(context.poolIdx);
                // Due to fences and multibuffering all queries should be ready for retrievel
                context.queryPool->GetResult(0, context.poolIdx, context.poolIdx * sizeof(uint64_t),
                    timeData.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

                for (auto& query : context.queries)
                    EvaluateQuery(context, query, timeData);

                history.history[history.historyIdx] = context.queries;

                history.historyIdx = (history.historyIdx + 1) % 64;
                newHistory[context.name] = history;

                unevaluatedThreadContexts.pop_front();
            }

            queryHistory = newHistory;

            supportsDebugMarkers = GraphicsDevice::DefaultDevice->support.debugMarker;

        }

        void Profiler::Shutdown() {

            threadContexts.clear();
            unevaluatedThreadContexts.clear();

        }

        void Profiler::BeginThread(const std::string &name, CommandList *commandList) {

            if (!enable) return;

            // We don't expect to have more than 2000 profiler queries
            auto queryPoolDesc = QueryPoolDesc {
                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = 2000
            };

            // Create thread context
            auto context = ThreadContext {
              .id = std::this_thread::get_id(),
              .name = name,
              .commandList = nullptr,
              .queryPool = GraphicsDevice::DefaultDevice->CreateQueryPool(queryPoolDesc),
              .frameIdx = frameIdx,
              .isValid = true
            };

            auto idx = threadContextCount.fetch_add(1);
            AE_ASSERT(idx < PROFILER_MAX_THREADS && "Too many threads for this frame");

            threadContexts[idx] = context;

            // Then set the command list
            if (commandList) SetCommandList(commandList);

        }

        void Profiler::EndThread() {

            if (!enable) return;

            std::lock_guard lock(unevaluatedThreadContextsMutex);

            auto& context = GetThreadContext();

            context.isValid = false;
            unevaluatedThreadContexts.push_back(context);

        }

        void Profiler::SetCommandList(CommandList *commandList) {

            if (!enable) return;

            auto& context = GetThreadContext();
            context.commandList = commandList;

        }

        void Profiler::BeginQuery(const std::string& name) {

            if (!enable) return;

            AE_ASSERT(name.length() > 0 && "Query names shouldn't be empty");

            auto& context = GetThreadContext();

            AE_ASSERT(context.commandList && "A command list must be set before \
                the first BeginQuery() call in the current thread");

            if (supportsDebugMarkers)
                context.commandList->BeginDebugMarker(name);

            Query query;

            query.name = name;
            query.timer.poolId = 0;
            query.timer.startId = context.poolIdx++;
            query.timer.endId = context.poolIdx++;
            query.stackLevel = context.stack.size() + 1;

            context.stack.push_back(query);

            context.commandList->Timestamp(context.queryPool, query.timer.startId);

        }

        void Profiler::EndQuery() {

            if (!enable) return;

            auto& context = GetThreadContext();

            AE_ASSERT(context.stack.size() && "Stack was empty. Maybe called EndQuery too many \
                times or code misses a BeginQuery.");

            if (supportsDebugMarkers)
                context.commandList->EndDebugMarker();

            auto query = context.stack.back();
            context.commandList->Timestamp(context.queryPool, query.timer.endId);

            context.stack.pop_back();

            // Check if we have a root query or if a parent
            // is available
            if (context.stack.size()) {
                // Add to the parent here instead of in BeginQuery()
                // to avoid divergence between stack elements through
                // dereferencing
                auto& parentQuery = context.stack.back();
                parentQuery.children.push_back(query);
            }
            else {
                context.queries.push_back(query);
            }
        }

        void Profiler::EndAndBeginQuery(const std::string& name) {

            if (!enable) return;

            EndQuery();
            BeginQuery(name);

        }

        std::vector<Profiler::ThreadData> Profiler::GetQueries(OrderBy order) {

            std::vector<ThreadData> threadData;
            for (auto& [name, threadHistory] : queryHistory) {
                ThreadData data;
                data.name = name;

                auto idx = threadHistory.historyIdx > 0 ? threadHistory.historyIdx - 1 : 64;
                data.queries = threadHistory.history[idx];

                if (order != OrderBy::CHRONO)
                    OrderQueries(data.queries, order);

                threadData.push_back(data);
            }

            return threadData;

        }

        std::vector<Profiler::ThreadData> Profiler::GetQueriesAverage(uint32_t frameCount, OrderBy order) {

            frameCount = std::min(frameCount, 64u);

            std::vector<ThreadData> threadData;
            for (auto& [name, threadHistory] : queryHistory) {
                ThreadData data;
                data.name = name;
                for (uint64_t i = 0; i < uint64_t(frameCount); i++)
                    AddQueriesToAverage(data.queries,
                        threadHistory.history[i], i, uint64_t(frameCount));

                if (order != OrderBy::CHRONO)
                    OrderQueries(data.queries, order);

                threadData.push_back(data);
            }

            return threadData;

        }

        void Profiler::EvaluateQuery(ThreadContext& context, Query& query, const std::vector<uint64_t>& timeData) {

            // Only evaluate query once
            if (query.timer.startId >= 0 && query.timer.endId >= 0) {
                query.timer.startTime = timeData[query.timer.startId];
                query.timer.endTime = timeData[query.timer.endId];

                query.timer.elapsedTime = query.timer.endTime - query.timer.startTime;

                query.timer.startId = -1;
                query.timer.endId = -1;
            }

            for (auto& child : query.children)
                EvaluateQuery(context, child, timeData);

        }

        void Profiler::OrderQueries(std::vector<Query>& queries, OrderBy order) {

            std::sort(queries.begin(), queries.end(),
                [=](Query q0, Query q1) {
                    if (order == OrderBy::MAX_TIME) {
                        return q0.timer.elapsedTime > q1.timer.elapsedTime;
                    }
                    else {
                        return q0.timer.elapsedTime < q1.timer.elapsedTime;
                    }
                });

            for (auto& query : queries)
                OrderQueries(query.children, order);

        }

        void Profiler::AddQueriesToAverage(std::vector<Query>& average,
            const std::vector<Query>& queries, uint64_t frameIdx, uint64_t frameCount) {

            if (!frameIdx) {
                average = queries;
                for (size_t i = 0; i < average.size(); i++) {
                    auto& avgQuery = average[i];
                    auto& query = queries[i];

                    avgQuery.timer.elapsedTime /= frameCount;
                    
                    AddQueriesToAverage(avgQuery.children,
                        query.children, frameIdx, frameCount);
                }
            }
            else {
                for (size_t i = 0; i < average.size(); i++) {
                    auto& avgQuery = average[i];

                    Query query;
                    if (i < queries.size())
                        query = queries[i];

                    // If the queries don't match up, just
                    // use the previous average
                    if (query.name != avgQuery.name) {
                        avgQuery.timer.elapsedTime += avgQuery.timer.elapsedTime / frameIdx;
                    }
                    else {
                        avgQuery.timer.elapsedTime += query.timer.elapsedTime / frameCount;
                    }

                    // Doesn't matter if the queries don't match up
                    AddQueriesToAverage(avgQuery.children,
                        query.children, frameIdx, frameCount);
                }
            }

        }

        Profiler::ThreadContext &Profiler::GetThreadContext() {
            auto threadId = std::this_thread::get_id();

            auto it = std::find_if(threadContexts.begin(), threadContexts.end(),
                [&](const ThreadContext& context) {
                    return threadId == context.id && context.isValid;
                });

            AE_ASSERT(it != threadContexts.end() && "Thread context not found. Missing a \
                BeginThread() call somewhere");

            return *it;

        }

    }

}