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

            std::mutex mutex;
        };



    }

}

#endif