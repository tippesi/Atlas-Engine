#pragma once

#include "Common.h"

#include <thread>

namespace Atlas {

    namespace Graphics {

        enum QueueType {
            GraphicsQueue = 0,
            PresentationQueue,
            TransferQueue
        };

        class GraphicsDevice;
        class CommandList;

        class Queue {

        public:
            bool IsTypeSupported(QueueType queueType) const;

            VkQueue queue;

            uint32_t familyIndex;
            uint32_t index;

            bool supportsGraphics;
            bool supportsTransfer;
            bool supportsPresentation;

            std::atomic<std::thread::id> threadId = std::thread::id();
            Ref<int32_t> counter = CreateRef(0);

            std::mutex mutex;
            std::mutex aquireMutex;

        };

        /**
         * Helper class for queues, just intended to be used in the thread it was retrieved.
         * Also all instances for the same queue are expected to be on one thread
         */
        class QueueRef {

            friend class GraphicsDevice;
            friend class CommandList;

        public:
            QueueRef() = default;

            ~QueueRef();

            void Unlock();

            VkQueue queue;
            uint32_t familyIndex;

        private:
            Ref<Queue> ref = nullptr;
            bool valid = false;

            Ref<int32_t> counter;

            QueueRef(Ref<Queue>& queue, std::thread::id threadId, bool forceLock);

        };

    }

}