#include "Profiler.h"

#include <algorithm>

namespace Atlas {

    void Profiler::OrderQueries(std::vector<Query>& queries, OrderBy order) {

        std::sort(queries.begin(), queries.end(), [order](const Query& query0, const Query& query1) {
            switch (order) {
            case OrderBy::MAX_TIME: return query0.elapsedTime > query1.elapsedTime;
            case OrderBy::MIN_TIME: return query0.elapsedTime < query1.elapsedTime;
            case OrderBy::NAME: return query0.name < query1.name;
            default: return query0.startTime < query1.startTime;
            }
            });

        for (auto& query : queries)
            OrderQueries(query.children, order);

    }

}
