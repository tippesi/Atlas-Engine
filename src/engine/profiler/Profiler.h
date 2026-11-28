#pragma once

#include "../System.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace Atlas {

    /**
     * The results of the CPU and the GPU profiler
     */
    class Profiler {

    public:
        /**
         * The time of a query in a frame or averaged over frames. Queries with the same name and parent are merged.
         */
        struct Query {
            std::string name;

            // Time stamp of the first call, orders the queries chronologically
            uint64_t startTime = 0;
            // Nanoseconds of all calls
            uint64_t elapsedTime = 0;
            uint32_t calls = 0;

            std::vector<Query> children;
        };

        struct ThreadData {
            std::string name;
            std::vector<Query> queries;
        };

        /**
         * Orders retrieved queries.
         * CHRONO: The queries have the order in which they were called
         * MAX_TIME: Order queries by their elapsed time in descending order
         * MIN_TIME: Order queries by their elapsed time in ascending order
         * NAME: Order queries alphabetically by their name
         */
        enum class OrderBy {
            CHRONO,
            MAX_TIME,
            MIN_TIME,
            NAME
        };

        /**
         * Measures from its construction to its destruction, used as CPUProfiler::Scope and GPUProfiler::Scope
         */
        template<class ProfilerType, class Name>
        class BasicScope {

        public:
            explicit BasicScope(Name name) : active(ProfilerType::BeginQuery(name)) {}

            ~BasicScope() { if (active) ProfilerType::EndQuery(); }

            // Ends the query and begins the next one, for consecutive sections
            void Next(Name name) {
                if (active)
                    ProfilerType::EndQuery();
                active = ProfilerType::BeginQuery(name);
            }

            BasicScope(const BasicScope&) = delete;

            BasicScope& operator=(const BasicScope&) = delete;

        private:
            bool active;

        };

    protected:
        // Adds a query to its siblings, the times, calls and children of queries with the same name add up
        template<class T>
        static void AddMerged(std::vector<T>& queries, T&& query) {

            for (auto& other : queries) {
                if (!SameName(other.name, query.name))
                    continue;

                other.startTime = std::min(other.startTime, query.startTime);
                other.elapsedTime += query.elapsedTime;
                other.calls += query.calls;
                for (auto& child : query.children)
                    AddMerged(other.children, std::move(child));
                return;
            }

            queries.push_back(std::move(query));

        }

        // Like above, but copies only the queries that aren't merged
        template<class T>
        static void AddMerged(std::vector<T>& queries, const T& query) {

            for (auto& other : queries) {
                if (!SameName(other.name, query.name))
                    continue;

                other.startTime = std::min(other.startTime, query.startTime);
                other.elapsedTime += query.elapsedTime;
                other.calls += query.calls;
                for (const auto& child : query.children)
                    AddMerged(other.children, child);
                return;
            }

            queries.push_back(query);

        }

        // Turns the merged queries of several frames into the average of a frame
        template<class T>
        static void Average(std::vector<T>& queries, uint32_t frameCount) {

            for (auto& query : queries) {
                query.elapsedTime /= frameCount;
                // A query of a single frame or one that spans frames still shows that it was called
                if (query.calls)
                    query.calls = std::max(1u, (query.calls + frameCount / 2) / frameCount);
                Average(query.children, frameCount);
            }

        }

        static void OrderQueries(std::vector<Query>& queries, OrderBy order);

    private:
        static bool SameName(const std::string& name0, const std::string& name1) { return name0 == name1; }

        static bool SameName(const char* name0, const char* name1) { return name0 == name1 || std::strcmp(name0, name1) == 0; }

    };

}
