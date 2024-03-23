#include "Queue.h"

namespace Atlas::Graphics {

    bool Queue::IsTypeSupported(QueueType queueType) const {
        switch (queueType) {
        case GraphicsQueue: return supportsGraphics;
        case PresentationQueue: return supportsPresentation;
        case TransferQueue: return supportsTransfer;
        default: return false;
        }
    }

    QueueRef::~QueueRef() {



    }

    QueueRef::QueueRef(Ref<Queue>& queue, std::thread::id threadId, bool forceLock) : ref(queue) {
        this->queue = ref->queue;
        this->familyIndex = ref->familyIndex;

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

    void QueueRef::Unlock() {

        // Don't unlock the same queue twice, could cause issues
        if (!valid)
            return;

        if (counter.use_count() == 2) {
            counter.reset();
            ref->threadId = std::thread::id();
            ref->mutex.unlock();
        }
        valid = false;

    }

}