#ifndef AE_VERTEXBUFFER_H
#define AE_VERTEXBUFFER_H

#include "../System.h"
#include "Buffer.h"
#include "../graphics/Buffer.h"

namespace Atlas {

	namespace Buffer {

		/**
		 * Manages the vertex data flow between the CPU and GPU
		 */
		class VertexBuffer {

		public:
			/**
			 * Constructs a VertexBuffer object.
			 */
			VertexBuffer() = default;

			/**
             * Constructs a VertexBuffer object.
             * @param format The format of the data
             * @param elementCount The number of elements in the vertex buffer will be filled with
			 * @param data Optional parameter for directly filling the buffer with data
             */
			VertexBuffer(VkFormat format, size_t elementCount, void* data = nullptr);

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

			VkFormat format;
            Ref<Graphics::Buffer> buffer;

            size_t elementCount = 0;
            size_t elementSize = 0;

        private:
            void Reallocate(void* data);

		};

	}

}

#endif