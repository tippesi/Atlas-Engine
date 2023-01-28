#ifndef AE_GRAPHICSQUERYPOOL_H
#define AE_GRAPHICSQUERYPOOL_H

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        struct QueryPoolDesc {
            VkQueryType queryType = VK_QUERY_TYPE_TIMESTAMP;
            uint32_t queryCount;
        };

        class QueryPool {
        public:
            QueryPool(GraphicsDevice* device, const QueryPoolDesc& desc);

            ~QueryPool();

            void GetResult(uint32_t firstQuery, uint32_t queryCount, size_t dataSize,
                void* result, size_t stride, VkQueryResultFlags flags = 0);

            void Reset();

            VkQueryPool pool;
            VkQueryType type;
            uint32_t queryCount;

        private:
            GraphicsDevice* device;

        };

    }

}

#endif
