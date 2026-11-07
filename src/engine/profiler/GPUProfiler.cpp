#include "GPUProfiler.h"
#include "../graphics/GraphicsDevice.h"

#include <algorithm>

namespace Atlas {

    std::shared_mutex GPUProfiler::threadContextsMutex;
    std::vector<Ref<GPUProfiler::ThreadContext>> GPUProfiler::threadContexts;

    std::mutex GPUProfiler::unevaluatedThreadContextsMutex;
    std::deque<GPUProfiler::ThreadContext> GPUProfiler::unevaluatedThreadContexts;

    std::unordered_map<std::string, GPUProfiler::ThreadHistory> GPUProfiler::queryHistory;
    std::atomic_size_t GPUProfiler::frameIdx = size_t(-1);

    bool GPUProfiler::supportsDebugMarkers = false;

#if !defined(AE_OS_MACOS) && !defined(AE_BUILDTYPE_RELEASE)
    std::atomic_bool GPUProfiler::enable = true;
#else
    std::atomic_bool GPUProfiler::enable = false;
#endif

    void GPUProfiler::BeginFrame() {

        {
            std::scoped_lock lock(threadContextsMutex);
            std::erase_if(threadContexts, [](const Ref<ThreadContext>& context) {
                return !enable || context->frameIdx + maxContextFrames < frameIdx;
                });
        }

        if (!enable) return;

        frameIdx++;

        std::scoped_lock lock(unevaluatedThreadContextsMutex);

        if (!unevaluatedThreadContexts.size()) return;

        // Each frame we replace the query history by a new history
        // This is to make sure old thread names are thrown out the window
        std::unordered_map<std::string, ThreadHistory> newHistory;
        while(unevaluatedThreadContexts.size() && unevaluatedThreadContexts.front().frameIdx + FRAME_DATA_COUNT <= frameIdx) {
            auto& context = unevaluatedThreadContexts.front();
            // The history gets moved, copying all queries of all frames each frame is expensive. Several
            // contexts of a thread continue the same history
            ThreadHistory history;
            if (auto iter = newHistory.find(context.name); iter != newHistory.end())
                history = std::move(iter->second);
            else if (auto oldIter = queryHistory.find(context.name); oldIter != queryHistory.end())
                history = std::move(oldIter->second);
            else
                history.history.resize(historyLength);

            std::vector<uint64_t> timeData(context.poolIdx);
            // Due to fences and multibuffering all queries should be ready for retrievel
            context.queryPool->GetResult(0, context.poolIdx, context.poolIdx * sizeof(uint64_t),
                timeData.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

            std::vector<Query> queries;
            for (const auto& query : context.queries)
                AddMerged(queries, EvaluateQuery(query, timeData));

            history.history[history.historyIdx] = std::move(queries);

            history.historyIdx = (history.historyIdx + 1) % historyLength;
            newHistory[context.name] = std::move(history);

            unevaluatedThreadContexts.pop_front();
        }

        // The queries of the last frames aren't necessarily ready yet, which is no reason to forget the older ones
        if (!newHistory.empty())
            queryHistory = std::move(newHistory);

        supportsDebugMarkers = Graphics::GraphicsDevice::DefaultDevice->support.debugMarker;

    }

    void GPUProfiler::Shutdown() {

        std::scoped_lock lock(threadContextsMutex, unevaluatedThreadContextsMutex);

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
        auto context = CreateRef<ThreadContext>(ThreadContext {
          .id = std::this_thread::get_id(),
          .name = name,
          .commandList = nullptr,
          .queryPool = Graphics::GraphicsDevice::DefaultDevice->CreateQueryPool(queryPoolDesc),
          .frameIdx = frameIdx
        });

        {
            std::scoped_lock lock(threadContextsMutex);
            threadContexts.push_back(context);
        }

        // Then set the command list
        if (commandList) SetCommandList(commandList);

    }

    void GPUProfiler::EndThread() {

        if (!enable) return;

        auto context = GetThreadContext();
        AE_ASSERT(context && "Thread context not found. Missing a BeginThread() call somewhere");
        if (!context) return;

        {
            std::scoped_lock lock(threadContextsMutex);
            std::erase(threadContexts, context);
        }

        // Readable once the frame this recording ended in is done, which isn't always the one it started in
        context->frameIdx = frameIdx;

        std::scoped_lock lock(unevaluatedThreadContextsMutex);
        unevaluatedThreadContexts.push_back(std::move(*context));

    }

    void GPUProfiler::SetCommandList(Graphics::CommandList *commandList) {

        if (!enable) return;

        auto context = GetThreadContext();
        AE_ASSERT(context && "Thread context not found. Missing a BeginThread() call somewhere");
        if (!context) return;

        context->commandList = commandList;

    }

    bool GPUProfiler::BeginQuery(const std::string& name) {

        if (!enable) return false;

        AE_ASSERT(name.length() > 0 && "Query names shouldn't be empty");

        auto context = GetThreadContext();
        AE_ASSERT(context && "Thread context not found. Missing a BeginThread() call somewhere");
        if (!context) return false;

        AE_ASSERT(context->commandList && "A command list must be set before \
            the first BeginQuery() call in the current thread");

        if (supportsDebugMarkers)
            context->commandList->BeginDebugMarker(name);

        TimestampQuery query;

        query.name = name;
        query.startId = context->poolIdx++;
        query.endId = context->poolIdx++;

        context->stack.push_back(query);

        context->commandList->Timestamp(context->queryPool, query.startId);

        return true;

    }

    void GPUProfiler::EndQuery() {

        // Queries that began are ended even if the profiler was disabled meanwhile
        auto contextRef = GetThreadContext();
        if (!contextRef || contextRef->stack.empty())
            return;

        auto& context = *contextRef;

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

    Ref<GPUProfiler::ThreadContext> GPUProfiler::GetThreadContext() {

        auto threadId = std::this_thread::get_id();

        std::shared_lock lock(threadContextsMutex);

        auto it = std::find_if(threadContexts.rbegin(), threadContexts.rend(),
            [&](const Ref<ThreadContext>& context) { return threadId == context->id; });

        return it != threadContexts.rend() ? *it : nullptr;

    }

}
