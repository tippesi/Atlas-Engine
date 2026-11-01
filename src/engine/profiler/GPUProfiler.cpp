#include "GPUProfiler.h"
#include "../graphics/GraphicsDevice.h"

#include <algorithm>

namespace Atlas {

    std::atomic_int GPUProfiler::threadContextCount = 0;
    std::vector<GPUProfiler::ThreadContext> GPUProfiler::threadContexts;

    std::mutex GPUProfiler::unevaluatedThreadContextsMutex;
    std::deque<GPUProfiler::ThreadContext> GPUProfiler::unevaluatedThreadContexts;

    std::unordered_map<std::string, GPUProfiler::ThreadHistory> GPUProfiler::queryHistory;
    size_t GPUProfiler::frameIdx = -1;

    bool GPUProfiler::supportsDebugMarkers = false;

#if !defined(AE_OS_MACOS) && !defined(AE_BUILDTYPE_RELEASE)
    std::atomic_bool GPUProfiler::enable = true;
#else
    std::atomic_bool GPUProfiler::enable = false;
#endif

    void GPUProfiler::BeginFrame() {

        threadContextCount = 0;
        threadContexts.clear();
        threadContexts.resize(GPU_PROFILER_MAX_THREADS);

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
                history.history.resize(historyLength);
            }

            std::vector<uint64_t> timeData(context.poolIdx);
            // Due to fences and multibuffering all queries should be ready for retrievel
            context.queryPool->GetResult(0, context.poolIdx, context.poolIdx * sizeof(uint64_t),
                timeData.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

            std::vector<Query> queries;
            for (const auto& query : context.queries)
                AddMerged(queries, EvaluateQuery(query, timeData));

            history.history[history.historyIdx] = std::move(queries);

            history.historyIdx = (history.historyIdx + 1) % historyLength;
            newHistory[context.name] = history;

            unevaluatedThreadContexts.pop_front();
        }

        queryHistory = newHistory;

        supportsDebugMarkers = Graphics::GraphicsDevice::DefaultDevice->support.debugMarker;

    }

    void GPUProfiler::Shutdown() {

        threadContexts.clear();
        unevaluatedThreadContexts.clear();

    }

    void GPUProfiler::BeginThread(const std::string &name, Graphics::CommandList *commandList) {

        if (!enable) return;

        // We don't expect to have more than 2000 profiler queries
        auto queryPoolDesc = Graphics::QueryPoolDesc {
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = 2000
        };

        // Create thread context
        auto context = ThreadContext {
          .id = std::this_thread::get_id(),
          .name = name,
          .commandList = nullptr,
          .queryPool = Graphics::GraphicsDevice::DefaultDevice->CreateQueryPool(queryPoolDesc),
          .frameIdx = frameIdx,
          .isValid = true
        };

        auto idx = threadContextCount.fetch_add(1);
        AE_ASSERT(idx < GPU_PROFILER_MAX_THREADS && "Too many threads for this frame");

        threadContexts[idx] = context;

        // Then set the command list
        if (commandList) SetCommandList(commandList);

    }

    void GPUProfiler::EndThread() {

        if (!enable) return;

        std::lock_guard lock(unevaluatedThreadContextsMutex);

        auto& context = GetThreadContext();

        context.isValid = false;
        unevaluatedThreadContexts.push_back(context);

    }

    void GPUProfiler::SetCommandList(Graphics::CommandList *commandList) {

        if (!enable) return;

        auto& context = GetThreadContext();
        context.commandList = commandList;

    }

    bool GPUProfiler::BeginQuery(const std::string& name) {

        if (!enable) return false;

        AE_ASSERT(name.length() > 0 && "Query names shouldn't be empty");

        auto& context = GetThreadContext();

        AE_ASSERT(context.commandList && "A command list must be set before \
            the first BeginQuery() call in the current thread");

        if (supportsDebugMarkers)
            context.commandList->BeginDebugMarker(name);

        TimestampQuery query;

        query.name = name;
        query.startId = context.poolIdx++;
        query.endId = context.poolIdx++;

        context.stack.push_back(query);

        context.commandList->Timestamp(context.queryPool, query.startId);

        return true;

    }

    void GPUProfiler::EndQuery() {

        // Queries that began are ended even if the profiler was disabled meanwhile
        auto threadId = std::this_thread::get_id();
        auto it = std::find_if(threadContexts.begin(), threadContexts.end(), [&](const ThreadContext& context) {
            return threadId == context.id && context.isValid;
            });
        if (it == threadContexts.end() || it->stack.empty())
            return;

        auto& context = *it;

        if (supportsDebugMarkers)
            context.commandList->EndDebugMarker();

        auto query = context.stack.back();
        context.commandList->Timestamp(context.queryPool, query.endId);

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

    void GPUProfiler::EndAndBeginQuery(const std::string& name) {

        if (!enable) return;

        EndQuery();
        BeginQuery(name);

    }

    std::vector<GPUProfiler::ThreadData> GPUProfiler::GetQueries(OrderBy order) {

        return GetThreadData(1, order);

    }

    std::vector<GPUProfiler::ThreadData> GPUProfiler::GetQueriesAverage(uint32_t frameCount, OrderBy order) {

        return GetThreadData(std::clamp(frameCount, 1u, historyLength), order);

    }

    GPUProfiler::Query GPUProfiler::EvaluateQuery(const TimestampQuery& query, const std::vector<uint64_t>& timeData) {

        Query result = {
            .name = query.name,
            .startTime = timeData[query.startId],
            .elapsedTime = timeData[query.endId] - timeData[query.startId],
            .calls = 1
        };

        for (const auto& child : query.children)
            AddMerged(result.children, EvaluateQuery(child, timeData));

        return result;

    }

    std::vector<GPUProfiler::ThreadData> GPUProfiler::GetThreadData(uint32_t frameCount, OrderBy order) {

        std::vector<ThreadData> threadData;
        for (const auto& [name, threadHistory] : queryHistory) {
            ThreadData data = { .name = name };
            for (uint32_t i = 1; i <= frameCount; i++) {
                const auto& frame = threadHistory.history[(threadHistory.historyIdx + historyLength - i) % historyLength];
                for (const auto& query : frame)
                    AddMerged(data.queries, query);
            }

            if (frameCount > 1)
                Average(data.queries, frameCount);
            OrderQueries(data.queries, order);
            threadData.push_back(std::move(data));
        }

        return threadData;

    }

    GPUProfiler::ThreadContext &GPUProfiler::GetThreadContext() {

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
