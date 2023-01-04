#ifndef AE_BUFFER_H
#define AE_BUFFER_H

#include "../System.h"

#include "../graphics/Buffer.h"

namespace Atlas {

	namespace OldBuffer {

        typedef uint32_t BufferUsage;

        typedef enum BufferUsageBits {
            StorageBuffer = (1 << 0),
            UniformBuffer = (1 << 1),
            MultiBuffered = (1 << 2),
            HostAccess = (1 << 3),
            MemoryTransfers = (1 << 4)
        }BufferUsageBits;

		/**
         * Base class for all other buffer classes. Can be used as standalone class.
         */
		class Buffer {

		public:
			/**
			 * Constructs a Buffer object.
			 */
			Buffer() = default;

			/**
             * Constructs a Buffer object.
             * @param bufferUsage How the buffer is intended to be used
             * @param elementSize The size of each element in the buffer
			 * @param elementCount The number of elements in the buffer.
			 * @param data Optional parameter for directly filling the buffer with data
             * @remark For more information on buffer types and flags see
             */
			Buffer(BufferUsage bufferUsage, size_t elementSize,
				size_t elementCount = 0, void* data = nullptr);

            /**
             * Returns a non-owning pointer to a graphics buffer
             * @return
             */
            Graphics::Buffer* Get();

			/**
             * Maps the data of the buffer into the memory.
             * @return Returns the pointer to the buffer memory
             * @note The mapping is only valid for the current frame
             */
			void* Map();

			/**
             * Unmaps the data of the buffer.
             * @note The mapping is only valid for the current frame and
             * needs to be unmapped before
             */
            void Unmap();

			/**
             * Sets the size of the buffer
             * @param elementCount The number of elements in the buffer
			 * @param data Optional parameter for directly filling the buffer
             * @note The size of the elements was defined when the buffer was
             * constructed. If the buffer has the BUFFER_DOUBLE_BUFFERING or
             * BUFFER_TRIPLE_BUFFERING flags the size of the buffer will be
             * 2 * elementsCount * elementSize or 3 * elementsCount * elementSize
             * respectively.
             */
			void SetSize(size_t elementCount, void* data = nullptr);

			/**
             * Sets the data of a buffer if it isn't mapped.
             * @param data A pointer to the data.
             * @param offset The offset in the buffer in elements (not bytes).
             * @param length The number of elements in data.
             */
			void SetData(void *data, size_t offset, size_t length);

			/**
             * Copies the data of the copy buffer to this buffer.
             * @param readBuffer Another buffer.
             * @param readOffset The offset in the readBuffer in bytes.
             * @param writeOffset The offset in the buffer in bytes.
             * @param length The length in bytes.
             * @note The read buffer should have a smaller or equal size than
             * the buffer.
             */
			void Copy(const Buffer *readBuffer, size_t readOffset,
				size_t writeOffset, size_t length);

			/**
             * Returns the type/target of the buffer
             * @return The type/target of the buffer as an integer
             */
			BufferUsage GetUsage();

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

		protected:
			void Reallocate(void* data);

            Ref<Graphics::Buffer> buffer;
            Ref<Graphics::MultiBuffer> multiBuffer;

			BufferUsage usage;

			size_t elementSize = 0;
			size_t elementCount = 0;

			size_t sizeInBytes = 0;

            bool multiBuffered = false;
            bool hostAccessible = false;

		};

	}

}

#endif