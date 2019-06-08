#ifndef AE_BUFFER_H
#define AE_BUFFER_H

#include "../System.h"
#include "../TypeFormat.h"
#include "BufferLock.h"

/**
 * If this bit is set the buffer will use double buffering. Use
 * Increment() to switch to the next part of the buffer.
 */
#define AE_BUFFER_DOUBLE_BUFFERING 2
/**
 * If this bit is set the buffer will use triple buffering. Use
 * Increment() to switch to the next part of the buffer.
 */
#define AE_BUFFER_TRIPLE_BUFFERING 4
/**
 * If this bit is set the buffer will use immutable storage
 * in case the system supports it. If this is the case persistent
 * mapped storage can be used, which maps the data of the buffer
 * into client memory while the applications runs and Unmap() hasn't
 * been called. Persistent mapped storage is only available if Map()
 * has been called on such a buffer and the BUFFER_DYNAMIC_STORAGE bit is set.
 */
#define AE_BUFFER_IMMUTABLE        8
/**
 * If this bit is set the buffer can be read when it is mapped.
 * @note This bit only affects the buffer if the BUFFER_DYNAMIC_STORAGE bit is set.
 */
#define AE_BUFFER_MAP_READ         16
/**
 * If this bit is set you can write to the buffer when it is mapped.
 * @note This bit only affects the buffer if the BUFFER_DYNAMIC_STORAGE bit is set.
 */
#define AE_BUFFER_MAP_WRITE        32
/**
 * If this bit is set you can access the buffer from clients side.
 * This means you can use SetData() and SetDataMapped() on the buffer.
 * If this isn't the case you can only access the buffer by copying something
 * from another buffer. You therefore use staging buffers.
 */
#define AE_BUFFER_DYNAMIC_STORAGE  64

#define AE_VERTEX_BUFFER			GL_ARRAY_BUFFER
#define AE_INDEX_BUFFER				GL_ELEMENT_ARRAY_BUFFER
#define AE_UNIFORM_BUFFER			GL_UNIFORM_BUFFER
#define AE_COMMAND_BUFFER			GL_DRAW_INDIRECT_BUFFER
#define AE_STAGING_BUFFER			GL_COPY_READ_BUFFER
#define AE_SHADER_STORAGE_BUFFER	GL_SHADER_STORAGE_BUFFER

namespace Atlas {

	namespace Buffer {

		/**
         * Base class for all other buffer classes. Can be used as standalone class.
         */
		class Buffer {

		public:
			/**
			 * Constructs a Buffer object.
			 */
			Buffer() {}

			/**
			 * Constructs a Buffer object.
			 * @param that Another Buffer object.
			 * @note The state of the other buffer won't be copied (e.g. if the
			 * other buffer is mapped this buffer won't be mapped automatically)
			 */
			Buffer(const Buffer& that);

			/**
             * Constructs a Buffer object.
             * @param type The type of the buffer, e.g VERTEX_BUFFER. See {@link Buffer.h} for more.
             * @param elementSize The size of each element in the buffer
             * @param flags The flags which should be used for the buffer. See {@link Buffer.h} for more.
             * @remark For more information on buffer types and flags see
             */
			Buffer(uint32_t type, size_t elementSize, uint32_t flags);

			virtual ~Buffer();

			/**
			 * Copies the data from another Buffer to the Buffer object.
			 * @param that Another Buffer object.
			 * @return A reference to the buffer.
			 * @note The state of the other buffer won't be copied (e.g. if the
			 * other buffer is mapped this buffer won't be mapped automatically)
			 */
			Buffer& operator=(const Buffer& that);

			/**
             * Binds the buffer to the target specified in the constructor.
             */
			virtual void Bind();

			/**
             * Binds a certain range of the buffer to the target.
             * @param offset The offset in the buffer in elements (not bytes).
             * @param length The number of elements to be bound.
             * @param base The base point the buffer should be bound to. Default is 0.
             */
			virtual void BindRange(size_t offset, size_t length, int32_t base = 0);

			/**
             * Binds the buffer to a base point
             * @param base The base point the buffer should be bound to.
             * @note This method is equivalent to the Bind() method, except that
             * it additionally binds the buffer to a base point.
             */
			virtual void BindBase(int32_t base);

			/**
             * Unbinds any buffer from the target.
             */
			virtual void Unbind();

			/**
             * Maps the data of the buffer into the memory.
             * @note The buffer has to be bound before.
             * @warning If the buffer isn't immutable you have to unmap the buffer
             * before calling any API function, e.g drawing with the buffer.
             * @remark If the buffer was constructed with BUFFER_IMMUTABLE and
             * if immutable storage is supported by the system, mapping the buffer
             * results in a persistent mapping, which means that data can be streamed
             * without ever calling Map() again. This even holds true if the buffer isn't
             * bound anymore. You can also do some API operations on the buffer, even if
             * it is bound.
             */
			virtual void Map();

			/**
             * Unmaps the data of the buffer.
             * @note The buffer has to be bound before.
             */
			virtual void Unmap();

			/**
             * If the buffer has the BUFFER_DOUBLE_BUFFERING or
             * BUFFER_TRIPLE_BUFFERING flags the data will be incremented.
             * @remark Let's assume that the BUFFER_TRIPLE_BUFFERING flag was set.
             * Then there is three times the data that was allocated with SetSize().
             * This gives you the possibility that you use some unused memory for each
             * new frame and that the possibility for a memory contention is relatively
             * low. Incrementing means that we have some data pointer p which points on
             * p = memoryOffset + (i % 3) * elementsCount * elementsSize, where i will
             * be incremented by every call to this method.
             */
			virtual void Increment();

			/**
             * Returns the index of the current increment.
             * @return The increment index as an integer
             * @remark Let's take the example described in Increment().
             * We have the memory pointer p for the buffer which points on
             * p = memoryOffset + (i % 3) * elementsCount * elementsSize, where i will
             * be incremented by calling Increment().
             * The index of the current increment equals (i % 3) in this example.
             */
			virtual int32_t GetIncrement();

			/**
             * Sets the size of the buffer
             * @param elementCount The number of elements in the buffer
             * @note The size of the elements was defined when the buffer was
             * constructed. If the buffer has the BUFFER_DOUBLE_BUFFERING or
             * BUFFER_TRIPLE_BUFFERING flags the size of the buffer will be
             * 2 * elementsCount * elementSize or 3 * elementsCount * elementSize
             * respectively.
             */
			void SetSize(size_t elementCount);

			/**
             * Sets the data of a buffer if it isn't mapped.
             * @param data A pointer to the data.
             * @param offset The offset in the buffer in elements (not bytes).
             * @param length The number of elements in data.
             */
			virtual void SetData(void *data, size_t offset, size_t length);

			/**
             * Sets the data of the buffer if it is mapped.
             * @param data A pointer to the data.
             * @param length The number of elements in data. Default is 1.
             */
			virtual void SetDataMapped(void *data, size_t length = 1);

			/**
             * Calculates the current position in the buffer in elements.
             * @return The number of written elements
             */
			virtual int32_t GetDataMappedAdvancement();

			/**
             * Copies the data of the copy buffer to this buffer.
             * @param readBuffer Another buffer.
             * @param readOffset The offset in the readBuffer in bytes.
             * @param writeOffset The offset in the buffer in bytes.
             * @param length The length in bytes.
             * @note The read buffer should have a smaller or equal size than
             * the buffer.
             */
			virtual void Copy(const Buffer *readBuffer, size_t readOffset, 
				size_t writeOffset, size_t length);

			/**
             * Returns the type/target of the buffer
             * @return The type/target of the buffer as an integer
             */
			virtual uint32_t GetType();

			/**
             * Returns the number of elements the buffer contains.
             * @return The number of elements.
             */
			virtual size_t GetElementCount();

			/**
             * Returns the size of each element in the buffer
             * @return The element size.
             */
			virtual size_t GetElementSize();

			/**
             * Returns the size in bytes of the buffer.
             * @return The size in bytes
             */
			virtual size_t GetSize();

			/**
             * Checks for extensions. Is being called by the engine
             */
			static void CheckExtensions();

			/**
             * Returns whether immutable storage and therefore persistent mapping is available.
             * @return True if immutable storage is supported, false otherwise
             */
			static bool IsImmutableStorageSupported();

		protected:
			virtual void DeepCopy(const Buffer& that);

			void CreateInternal();

			void DestroyInternal();

			uint32_t ID = 0;

			uint32_t type = 0;

			size_t elementSize = 0;
			size_t elementCount = 0;

			size_t sizeInBytes = 0;

			uint32_t flags = 0;
			uint32_t mapFlags = 0;
			uint32_t dataFlags = 0;

			bool mapped = false;
			size_t mappedData = 0;
			size_t mappedDataOffset = 0;

			bool immutable = false;
			bool dynamicStorage = false;

			size_t bufferingCount = 0;
			size_t bufferingIndex = 0;

			BufferLock bufferLock;

			static bool immutableStorageSupported;

		};

	}

}

#endif