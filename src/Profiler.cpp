#include "Profiler.h"

#include <algorithm>
#include <set>
#include <iterator>

namespace Atlas {

	std::vector<Profiler::Query> Profiler::stack;

	std::vector<Profiler::Query> Profiler::queries;

	std::vector<std::vector<Profiler::Query>> Profiler::queryHistory;
	size_t Profiler::frameIdx = 0;

	bool Profiler::activate = true;

	void Profiler::BeginQuery(const std::string& name) {

		if (!activate) return;

		assert(name.length() > 0 && "Query names shoudn't be empty");

		uint32_t ids[2];
		glGenQueries(2, &ids[0]);
		glQueryCounter(ids[0], GL_TIMESTAMP);

		Query query;

		query.name = name;
		query.timer.startId = int32_t(ids[0]);
		query.timer.endId = int32_t(ids[1]);
		query.stackLevel = stack.size() + 1;

		stack.push_back(query);

	}

	void Profiler::EndQuery() {

		if (!activate) return;

		assert(stack.size() && "Stack was empty. Maybe called EndQuery too many \
			times or code misses a BeginQuery.");

		auto query = stack.back();
		glQueryCounter(query.timer.endId, GL_TIMESTAMP);

		stack.pop_back();

		// Check if we have a root query or if a parent
		// is available
		if (stack.size()) {
			// Add to the parent here instead of in BeginQuery()
			// to avoid divergence between stack elements through
			// dereferencing
			auto& parentQuery = stack.back();
			parentQuery.children.push_back(query);
		}
		else {
			queries.push_back(query);
		}
	}

	void Profiler::EndAndBeginQuery(const std::string& name) {

		if (!activate) return;

		EndQuery();
		BeginQuery(name);

	}

	std::vector<Profiler::Query> Profiler::GetQueries(OrderBy order) {

		EvaluateHistroy();
        auto queries = GetLatestHistory();

		if (order != OrderBy::CHRONO)
			OrderQueries(queries, order);

		return queries;

	}

	std::vector<Profiler::Query> Profiler::GetQueriesAverage(uint32_t frameCount, OrderBy order) {

		EvaluateHistroy();
		std::vector<Query> average;

		frameCount = std::min(frameCount, 64u);

		for (uint64_t i = 0; i < uint64_t(frameCount); i++)
			average = AddQueriesToAverage(average, queryHistory[i], i, uint64_t(frameCount));

		if (order != OrderBy::CHRONO)
			OrderQueries(average, order);

		return average;

	}

	void Profiler::Update() {

		assert(!stack.size() && "Stack should be empty at the beginning of a new \
			frame. There seems to be a missing EndQuery()");

		UpdateHistory();
		
		queries.clear();

	}

	void Profiler::EvaluateQuery(Query& query) {

		// Only evaluate query once
		if (query.timer.startId >= 0 && query.timer.endId >= 0) {
			// This will stall the pipeline if the results aren't available yet
			glGetQueryObjectui64v(query.timer.startId, GL_QUERY_RESULT, &query.timer.startTime);
			glGetQueryObjectui64v(query.timer.endId, GL_QUERY_RESULT, &query.timer.endTime);

			query.timer.elapsedTime = query.timer.endTime - query.timer.startTime;

			uint32_t ids[] = { uint32_t(query.timer.startId), uint32_t(query.timer.endId) };
			glDeleteQueries(2, &ids[0]);

			query.timer.startId = -1;
			query.timer.endId = -1;
		}

		for (auto& child : query.children)
			EvaluateQuery(child);

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

	void Profiler::UpdateHistory() {

		if (!queryHistory.size()) {
			queryHistory.resize(64);
		}

		queryHistory[frameIdx] = queries;

		frameIdx = (frameIdx + 1) % 64;

	}

	void Profiler::EvaluateHistroy() {

		for (auto& queries : queryHistory)
			for (auto& query : queries)
				EvaluateQuery(query);

	}

	std::vector<Profiler::Query> Profiler::GetLatestHistory() {

		auto idx = frameIdx > 0 ? frameIdx - 1 : 64;
		return queryHistory[idx];

	}

	std::vector<Profiler::Query> Profiler::AddQueriesToAverage(std::vector<Query> average,
		std::vector<Query> queries,	uint64_t frameIdx, uint64_t frameCount) {

		if (!frameIdx) {
			average = queries;
			for (size_t i = 0; i < average.size(); i++) {
				auto& avgQuery = average[i];
				auto& query = queries[i];

				avgQuery.timer.elapsedTime /= frameCount;

				avgQuery.children = AddQueriesToAverage(avgQuery.children,
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
				avgQuery.children = AddQueriesToAverage(avgQuery.children,
					query.children, frameIdx, frameCount);
			}			
		}

		return average;

	}

}