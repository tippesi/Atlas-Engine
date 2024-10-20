#pragma once

#include "Common.h"
#include "RenderPass.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "Image.h"
#include "Sampler.h"
#include "DescriptorSetLayout.h"
#include "DescriptorPool.h"
#include "QueryPool.h"
#include "Framebuffer.h"
#include "BLAS.h"
#include "TLAS.h"
#include "MemoryTransferManager.h"

#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

#include <deque>
#include <cassert>
#include <functional>

namespace Atlas {

    namespace Graphics {

        // Forward declare such that it can be a friend
        class GraphicsDevice;
        class MemoryManager;

        template<typename T>
        class DeleteResource {
        public:
            Ref<T> resource;
            size_t deleteFrame;
        };

        class DeleteLambda {
        public:
            std::function<void(void)> lambda;
            size_t deleteFrame;
        };

        class MemoryManager {

            friend GraphicsDevice;

        public:
            MemoryManager(GraphicsDevice* device);

            MemoryManager(const MemoryManager& that) = delete;

            ~MemoryManager();

            MemoryManager& operator=(const MemoryManager& that) = delete;

            void DestroyAllocation(Ref<RenderPass>& allocation);

            void DestroyAllocation(Ref<FrameBuffer>& allocation);

            void DestroyAllocation(Ref<Shader>& allocation);

            void DestroyAllocation(Ref<Pipeline>& allocation);

            void DestroyAllocation(Ref<Buffer>& allocation);

            void DestroyAllocation(Ref<MultiBuffer>& allocation);

            void DestroyAllocation(Ref<Image>& allocation);

            void DestroyAllocation(Ref<Sampler>& allocation);

            void DestroyAllocation(Ref<DescriptorSetLayout>& allocation);

            void DestroyAllocation(Ref<DescriptorPool>& allocation);

            void DestroyAllocation(Ref<QueryPool>& allocation);

            void DestroyAllocation(Ref<BLAS>& allocation);

            void DestroyAllocation(Ref<TLAS>& allocation);

            void DestroyRawAllocation(std::function<void(void)> destroyLambda);

            void DestroyAllImmediate();

            VmaAllocator allocator;
            VmaPool highPriorityMemoryPool;
            VmaPool highPriorityRenderTargetPool;

            VkPhysicalDeviceProperties deviceProperties;

            GraphicsDevice* device;
            MemoryTransferManager* transferManager;

        private:
            void UpdateFrameIndex(size_t frameIndex);

            void DeleteData();

            template<class T>
            void DeleteAllocations(std::deque<DeleteResource<T>>& deleteAllocations) {
                while (deleteAllocations.size() &&
                    deleteAllocations.front().deleteFrame <= frameIndex) {
                    auto &allocation = deleteAllocations.front();

                    // This should never happen
                    AE_ASSERT(allocation.resource.use_count() == 1 && "Resource allocation is not uniquely owned");
                    allocation.resource.reset();

                    deleteAllocations.pop_front();
                }
            }

            const size_t framesToDeletion = 3;

            size_t frameIndex = 0;
            std::deque<DeleteLambda> deleteRawAllocations;
            std::deque<DeleteResource<RenderPass>> deleteRenderPassAllocations;
            std::deque<DeleteResource<FrameBuffer>> deleteFrameBufferAllocations;
            std::deque<DeleteResource<Shader>> deleteShaderAllocations;
            std::deque<DeleteResource<Pipeline>> deletePipelineAllocations;
            std::deque<DeleteResource<Buffer>> deleteBufferAllocations;
            std::deque<DeleteResource<MultiBuffer>> deleteMultiBufferAllocations;
            std::deque<DeleteResource<Image>> deleteImageAllocations;
            std::deque<DeleteResource<Sampler>> deleteSamplerAllocations;
            std::deque<DeleteResource<DescriptorSetLayout>> deleteDescriptorSetLayoutAllocations;
            std::deque<DeleteResource<DescriptorPool>> deleteDescriptorPoolAllocations;
            std::deque<DeleteResource<QueryPool>> deleteQueryPoolAllocations;
            std::deque<DeleteResource<BLAS>> deleteBLASAllocations;
            std::deque<DeleteResource<TLAS>> deleteTLASAllocations;

        };

    }

}