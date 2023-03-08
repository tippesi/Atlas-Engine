#include "QueryPool.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        QueryPool::QueryPool(GraphicsDevice *device, const QueryPoolDesc &desc) :
            type(desc.queryType), device(device), queryCount(desc.queryCount) {

            VkQueryPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            poolInfo.pNext = nullptr;
            poolInfo.flags = 0;
            poolInfo.queryType = desc.queryType;
            poolInfo.queryCount = desc.queryCount;
            poolInfo.pipelineStatistics = 0;

            VK_CHECK(vkCreateQueryPool(device->device, &poolInfo, nullptr, &pool))
            Reset();

        }

        QueryPool::~QueryPool() {

            vkDestroyQueryPool(device->device, pool, nullptr);

        }

        bool QueryPool::GetResult(uint32_t firstQuery, uint32_t queryCount, size_t dataSize,
            void *result, size_t stride, VkQueryResultFlags flags) {

            bool success = vkGetQueryPoolResults(device->device, pool, firstQuery,
                queryCount, dataSize, result, stride, flags) == VK_SUCCESS;

            return success;

        }

        void QueryPool::Reset() {

            vkResetQueryPool(device->device, pool, 0, queryCount);

        }

    }

}