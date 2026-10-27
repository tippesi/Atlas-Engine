#include "UploadBufferPool.h"
#include "GraphicsDevice.h"

namespace Atlas {

    namespace Graphics {

        Ref<Buffer> UploadBufferPool::Upload(const void* data, size_t size) {

            auto device = GraphicsDevice::DefaultDevice;
            auto frameIndex = device->GetFrameIndex();

            std::lock_guard lock(mutex);

            auto& frame = frames[frameIndex % FRAME_DATA_COUNT];
            if (frame.frameIndex != frameIndex) {
                frame.frameIndex = frameIndex;
                frame.next = 0;
            }

            if (frame.next == frame.buffers.size())
                frame.buffers.emplace_back();

            auto& buffer = frame.buffers[frame.next++];
            if (!buffer || buffer->size < size) {
                BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .domain = BufferDomain::Host,
                    .size = std::max(size + size / 2, size_t(256))
                };
                buffer = device->CreateBuffer(desc);
            }

            if (size)
                buffer->SetData(const_cast<void*>(data), 0, size);

            return buffer;

        }

    }

}
