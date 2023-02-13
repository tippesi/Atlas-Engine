#ifndef AE_UNIFORMBUFFER_H
#define AE_UNIFORMBUFFER_H

#include "../System.h"

#include "../graphics/Buffer.h"

namespace Atlas {

    namespace Buffer {

        class UniformBuffer {

        public:
            /**
             * Constructs a UniformBuffer object.
             */
            UniformBuffer() = default;

            /**
             * Constructs a UniformBuffer object.
             * @param format The format of the data
             * @param elementCount The number of elements in the vertex buffer will be filled with
			 * @param hostAccess An optional parameter to specify if the buffer is accessed by the host
             */
            UniformBuffer(size_t elementSize, size_t elementCount = 1, bool hostAccess = true);

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

            /**
             * Returns an owning pointer to a graphics multi buffer
             * @return
             */
            Ref<Graphics::MultiBuffer> Get();

            /**
             * Returns the number of elements the buffer contains.
             * @return The number of elements.
             */
            size_t GetElementCount();

            /**
             * Returns the size of each element in the buffer
             * @return The element size.
             */
            size_t GetElementSize();

            /**
             * Returns the size in bytes of the buffer.
             * @return The size in bytes
             */
            size_t GetSize();

            /**
             * Returns the aligned offset for dynamic uniform buffer binding
             * @param elementIndex The offset in elements
             * @return The offset in bytes
             */
            size_t GetAlignedOffset(size_t elementIndex);

            bool hostAccess = true;

        private:
            void Reallocate(void* data);

            Ref<Graphics::MultiBuffer> buffer;

            size_t elementCount = 0;
            size_t elementSize = 0;

        };

    }

}

#endif