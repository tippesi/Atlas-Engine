#ifndef AE_PROFILER_H
#define AE_PROFILER_H

#include "System.h"

#include <vector>

namespace Atlas {

    /**
     * GPU Profiling class
     */
	class Profiler {

	public:
        /**
         * Measures the elapsed time between two points
         */
		struct Timer {
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
		static std::vector<Query> GetQueries(OrderBy order = OrderBy::CHRONO);

        /**
         * Gets the averaged queries of the last n frames
         * @param frameCount The amount of frames to use in the average
         * @param order The order of the query. Chronologically be default.
         * @return 
         */
		static std::vector<Query> GetQueriesAverage(uint32_t frameCount = 32,
			OrderBy order = OrderBy::CHRONO);

		/**
		 * Update method called every frame by the engine.
		 * @note This method is used internally two swap
		 * the queries.
		 */
		static void Update();

	private:
		static void EvaluateQuery(Query& query);

		static void OrderQueries(std::vector<Query>& queries, OrderBy order);

		static void UpdateHistory();

        static void EvaluateHistroy();

        static std::vector<Query> GetLatestHistory();

		static std::vector<Query> AddQueriesToAverage(std::vector<Query> average,
			std::vector<Query> queries,	uint64_t frameIdx, uint64_t frameCount);

		static std::vector<Query> stack;
		static std::vector<Query> queries;

		static std::vector<std::vector<Query>> queryHistory;
		static size_t frameIdx;

        static bool activate;

	};

}

#endif