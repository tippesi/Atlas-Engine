#pragma once

#include "Common.h"
#include "Buffer.h"

#include <mutex>
#include <vector>

namespace Atlas {

    namespace Graphics {

        /**
         * Host buffers for data which is uploaded while recording. The scene can be updated and rendered several times
         * per frame, so every upload of a frame gets its own buffer. The buffers of a frame are reused once it is done
         * on the GPU.
         */
        class UploadBufferPool {

        public:
            Ref<Buffer> Upload(const void* data, size_t size);

        private:
            struct FrameBuffers {
                int32_t frameIndex = -1;
                size_t next = 0;
                std::vector<Ref<Buffer>> buffers;
            };

            FrameBuffers frames[FRAME_DATA_COUNT];
            std::mutex mutex;

        };

    }

}
