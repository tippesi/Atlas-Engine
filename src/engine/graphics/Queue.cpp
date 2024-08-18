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

        {
            std::scoped_lock locked(ref->aquireMutex);
            if (threadId == ref->threadId && ref->counter > 0) {
                this->counter = ref->counter;
                valid = true;
                return;
            }
        }

        if (forceLock) {
            ref->mutex.lock();
            ref->threadId = threadId;
            this->counter = ref->counter;
            valid = true;
        }
        else {
            if (ref->mutex.try_lock()) {
                ref->threadId = threadId;
                this->counter = ref->counter;
                valid = true;
            }
        }
    }

    void QueueRef::Unlock() {

        // Don't unlock the same queue twice, could cause issues
        if (!valid)
            return;

        std::scoped_lock lock(ref->aquireMutex);
        if (counter.use_count() == 2) {
            counter.reset();
            ref->threadId = std::thread::id();
            ref->mutex.unlock();
        }
        valid = false;

    }

}
