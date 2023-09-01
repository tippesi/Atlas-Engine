#pragma once

#include "../System.h"
#include "Buffer.h"
#include "../graphics/Buffer.h"

namespace Atlas {

    namespace Buffer {

        class IndexBuffer {

        public:
            /**
             * Constructs an IndexBuffer object.
             */
            IndexBuffer() = default;

            /**
             * Constructs an IndexBuffer object.
             * @param type The data type of the data
             * @param elementCount The number of elements in the vertex buffer will be filled with
             * @param data Optional parameter for directly filling the buffer with data
             */
            IndexBuffer(VkIndexType type, size_t elementCount, void* data = nullptr,
                bool hostAccess = false);

            /**
             * Sets the size of the buffer
             * @param elementCount The number of elements in the buffer
             * @param data Optional parameter for directly filling the buffer
             */
            void SetSize(size_t elementCount, void* data = nullptr);

            /**
             * Sets the data of a buffer if it isn't mapped.
             * @param data A pointer to the data.
             * @param offset The offset in the buffer in elements (not bytes).
             * @param length The number of elements in data.
             */
            void SetData(void* data, size_t offset, size_t length);

            VkIndexType type;
            Ref<Graphics::Buffer> buffer;

            size_t elementCount = 0;
            size_t elementSize = 0;

            bool hostAccessible = false;

        private:
            void Reallocate(void* data);

        };

    }

}