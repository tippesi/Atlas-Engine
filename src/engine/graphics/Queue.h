#ifndef AE_GRAPHICQUEUE_H
#define AE_GRAPHICQUEUE_H

#include "Common.h"

namespace Atlas {

    namespace Graphics {

        enum QueueType {
            GraphicsQueue = 0,
            PresentationQueue,
            TransferQueue
        };

        struct Queue {
            VkQueue queue;

            uint32_t familyIndex;
            uint32_t index;

            std::mutex mutex;
        };



    }

}

#endif