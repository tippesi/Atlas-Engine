#pragma once

#include "Common.h"

#include <thread>
#include <sstream>

namespace Atlas {

    namespace Graphics {

        enum QueueType {
            GraphicsQueue = 0,
            PresentationQueue,
            TransferQueue
        };

        class GraphicsDevice;

        class Queue {

        public:
            bool IsTypeSupported(QueueType queueType) const {
                switch (queueType) {
                case GraphicsQueue: return supportsGraphics;
                case PresentationQueue: return supportsPresentation;
                case TransferQueue: return supportsTransfer;
                default: return false;
                }
            }

            VkQueue queue;

            uint32_t familyIndex;
            uint32_t index;

            bool supportsGraphics;
            bool supportsTransfer;
            bool supportsPresentation;

            std::thread::id threadId = std::thread::id();
            Ref<int32_t> counter = CreateRef(0);

            std::mutex mutex;

        };

        /**
         * Helper class for queues, just intended to be used in the thread it was retrieved.
         * Also all instances for the same queue are expected to be on one thread
         */
        class QueueRef {

            friend class GraphicsDevice;

        public:
            QueueRef() = default;

            ~QueueRef() {
                Unlock();
            }

            void Unlock() {
                if (counter.use_count() == 2) {
                    counter.reset();
                    ref->threadId = std::thread::id();
                    ref->mutex.unlock();
                }
                valid = false;

                std::stringstream ss;
                ss << ref->threadId;
                std::string mystring = ss.str();

                std::stringstream as;
                as << ref->queue;
                std::string mystring2 = as.str();

                Log::Warning("Unlock queue " + mystring2 + "from index " + std::to_string(familyIndex) + " with id " + std::to_string(ref->index) + " from thred " + mystring);
            }

            VkQueue queue;
            uint32_t familyIndex;

        private:
            Ref<Queue> ref = nullptr;
            bool valid = false;

            Ref<int32_t> counter;

            QueueRef(Ref<Queue>& queue, std::thread::id threadId, bool forceLock) : ref(queue) {
                this->queue = ref->queue;
                this->familyIndex = ref->familyIndex;

                std::stringstream ss;
                ss << threadId;
                std::string mystring = ss.str();

                std::stringstream as;
                as << ref->queue;
                std::string mystring2 = as.str();

                Log::Warning("Lock queue " + mystring2 + "from index " + std::to_string(familyIndex) + " with id " + std::to_string(ref->index) + " from thred " + mystring);

                if (threadId == queue->threadId && queue->counter > 0) {
                    this->counter = queue->counter;
                    valid = true;
                    return;
                }

                if (forceLock) {
                    queue->mutex.lock();
                    queue->threadId = threadId;
                    this->counter = queue->counter;
                    valid = true;
                }
                else {
                    if (queue->mutex.try_lock()) {
                        queue->threadId = threadId;
                        this->counter = queue->counter;
                        valid = true;
                    }
                }
            }

        };

    }

}