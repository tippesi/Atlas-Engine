#include "CPUProfiler.h"

#include <algorithm>
#include <tuple>

namespace Atlas {

#ifndef AE_BUILDTYPE_RELEASE
    std::atomic_bool CPUProfiler::enable = true;
#else
    std::atomic_bool CPUProfiler::enable = false;
#endif

    std::mutex CPUProfiler::threadsMutex;
    std::vector<std::unique_ptr<CPUProfiler::ThreadBuffer>> CPUProfiler::threads;
    uint32_t CPUProfiler::threadCount = 0;

    std::mutex CPUProfiler::historyMutex;
    std::unordered_map<std::string, CPUProfiler::ThreadHistory> CPUProfiler::history;
    uint64_t CPUProfiler::frameIdx = 0;

    // The ticks are converted with their ratio to the steady clock since the start, which gets more exact over time
    uint64_t CPUProfiler::startTicks = CPUProfiler::Now();
    std::chrono::steady_clock::time_point CPUProfiler::startTime = std::chrono::steady_clock::now();
    double CPUProfiler::nanosecondsPerTick = 1.0;

    // The name of a thread that doesn't measure yet, its buffer is only allocated with the first query
    static thread_local std::string threadName;
    static thread_local bool threadEnded = false;

    // Marks the buffer of an ending thread, it is removed once its last queries are collected. Queries while the
    // thread is torn down aren't measured
    struct CPUProfiler::ThreadRelease {
        ThreadBuffer* buffer = nullptr;

        ~ThreadRelease() {
            if (buffer)
                buffer->released = true;
            threadBuffer = nullptr;
            threadEnded = true;
        }
    };

    void CPUProfiler::BeginFrame() {

        auto ticks = Now();
        auto time = std::chrono::steady_clock::now();
        if (ticks > startTicks)
            nanosecondsPerTick = double(std::chrono::duration_cast<std::chrono::nanoseconds>(time - startTime).count()) /
                double(ticks - startTicks);

        std::scoped_lock lock(threadsMutex, historyMutex);

        frameIdx++;
        for (auto& buffer : threads) {
            if (enable) {
                Collect(*buffer);
            }
            else {
                buffer->read = buffer->written.load(std::memory_order_acquire);
                buffer->pending.clear();
            }
        }

        std::erase_if(threads, [](const auto& buffer) { return buffer->released.load(); });
        // Threads that ended or don't measure anything anymore leave the results after a while
        std::erase_if(history, [](const auto& item) { return item.second.lastFrame + historyLength < frameIdx; });

    }

    void CPUProfiler::SetThreadName(const std::string& name) {

        if (!threadBuffer) {
            threadName = name;
            return;
        }

        std::scoped_lock lock(threadsMutex);
        threadBuffer->name = name;

    }

    std::vector<CPUProfiler::ThreadData> CPUProfiler::GetQueries(OrderBy order) {

        return GetThreadData(1, order);

    }

    std::vector<CPUProfiler::ThreadData> CPUProfiler::GetQueriesAverage(uint32_t frameCount, OrderBy order) {

        return GetThreadData(std::clamp(frameCount, 1u, historyLength), order);

    }

    CPUProfiler::ThreadBuffer* CPUProfiler::RegisterThread() {

        if (threadEnded)
            return nullptr;

        auto buffer = std::make_unique<ThreadBuffer>();
        buffer->events.resize(eventCapacity);
        threadBuffer = buffer.get();

        static thread_local ThreadRelease release;
        release.buffer = threadBuffer;

        std::scoped_lock lock(threadsMutex);
        buffer->order = threadCount++;
        buffer->name = threadName.empty() ? "Thread " + std::to_string(buffer->order) : threadName;
        threads.push_back(std::move(buffer));

        return threadBuffer;

    }

    void CPUProfiler::Collect(ThreadBuffer& buffer) {

        // The events are copied first: the thread keeps writing and overwrites the oldest ones when it is faster
        // than the collection, those are dropped before their names are read
        auto written = buffer.written.load(std::memory_order_acquire);
        auto first = std::max(buffer.read, written > eventCapacity ? written - eventCapacity : uint64_t(0));
        std::vector<Event> events;
        events.reserve(written - first);
        for (auto i = first; i < written; i++)
            events.push_back(buffer.events[i & (eventCapacity - 1)]);

        // The slot of the event that is written right now is also the slot of the oldest valid one
        std::atomic_thread_fence(std::memory_order_acquire);
        auto overwritten = buffer.written.load(std::memory_order_relaxed);
        auto valid = overwritten + 1 > eventCapacity ? overwritten + 1 - eventCapacity : uint64_t(0);
        auto skip = valid > first ? std::min(valid - first, uint64_t(events.size())) : uint64_t(0);

        // The parents of children that are waiting might have been dropped
        if (first > buffer.read || skip)
            buffer.pending.clear();
        buffer.read = written;

        std::vector<Node> roots;
        for (auto i = size_t(skip); i < events.size(); i++) {
            const auto& event = events[i];
            // A torn event, only possible when it was overwritten while it was copied
            if (event.end < event.start || event.depth >= maxDepth || !event.name)
                continue;

            Node node = {
                .name = event.name,
                .startTime = event.start > startTicks ? uint64_t(double(event.start - startTicks) * nanosecondsPerTick) : 0,
                .elapsedTime = uint64_t(double(event.end - event.start) * nanosecondsPerTick),
                .calls = 1
            };

            // Events end after their children, the queries of the next depth that are waiting belong to this one
            if (buffer.pending.size() < size_t(event.depth) + 2)
                buffer.pending.resize(size_t(event.depth) + 2);
            node.children = std::move(buffer.pending[event.depth + 1]);
            buffer.pending[event.depth + 1].clear();

            AddMerged(event.depth ? buffer.pending[event.depth] : roots, std::move(node));
        }

        if (roots.empty() && !history.contains(buffer.name))
            return;

        auto& threadHistory = history[buffer.name];
        if (threadHistory.frames.empty())
            threadHistory.frames.resize(historyLength);
        threadHistory.order = buffer.order;
        threadHistory.frames[threadHistory.index] = std::move(roots);
        threadHistory.index = (threadHistory.index + 1) % historyLength;
        if (!threadHistory.frames[(threadHistory.index + historyLength - 1) % historyLength].empty())
            threadHistory.lastFrame = frameIdx;

    }

    CPUProfiler::Query CPUProfiler::ToQuery(const Node& node) {

        Query query = {
            .name = node.name,
            .startTime = node.startTime,
            .elapsedTime = node.elapsedTime,
            .calls = node.calls
        };
        query.children.reserve(node.children.size());
        for (const auto& child : node.children)
            query.children.push_back(ToQuery(child));

        return query;

    }

    std::vector<CPUProfiler::ThreadData> CPUProfiler::GetThreadData(uint32_t frameCount, OrderBy order) {

        std::vector<std::tuple<uint32_t, std::string, std::vector<Node>>> threadNodes;

        {
            std::scoped_lock lock(historyMutex);
            for (const auto& [name, threadHistory] : history) {
                std::vector<Node> nodes;
                for (uint32_t i = 1; i <= frameCount; i++) {
                    auto& frame = threadHistory.frames[(threadHistory.index + historyLength - i) % historyLength];
                    for (const auto& node : frame)
                        AddMerged(nodes, node);
                }
                threadNodes.emplace_back(threadHistory.order, name, std::move(nodes));
            }
        }

        std::sort(threadNodes.begin(), threadNodes.end(), [](const auto& data0, const auto& data1) {
            return std::get<0>(data0) < std::get<0>(data1);
            });

        std::vector<ThreadData> threadData;
        for (auto& [threadOrder, name, nodes] : threadNodes) {
            if (frameCount > 1)
                Average(nodes, frameCount);

            ThreadData data = { .name = name };
            for (const auto& node : nodes)
                data.queries.push_back(ToQuery(node));
            OrderQueries(data.queries, order);
            threadData.push_back(std::move(data));
        }

        return threadData;

    }

}
