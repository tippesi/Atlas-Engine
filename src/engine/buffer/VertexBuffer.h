#ifndef AE_VERTEXBUFFER_H
#define AE_VERTEXBUFFER_H

#include "../System.h"
#include "Buffer.h"

namespace Atlas {

	namespace Buffer {

		/**
		 * Manages the vertex data flow between the CPU and GPU
		 */
		class VertexBuffer : public Buffer {

		public:
			/**
			 * Constructs a VertexBuffer object.
			 */
			VertexBuffer() = default;

			/**
             * Constructs a VertexBuffer object.
             * @param dataType The data type of the data, e.g AE_FLOAT
             * @param stride The number of elements of dataType per element
             * @param elementSize The size of each element in bytes
             * @param elementCount The number of elements in the vertex buffer will be filled with
			 * @param data Optional parameter for directly filling the buffer with data
             * @param flags The flags of the vertex buffer. Shouldn't be changed unless it's really needed. See {@link Buffer.h} for more.
             * @note This is similar to
             * <a href="https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBufferData.xhtml">glBufferData</a>.
             * For all available data types see <a href="https://www.khronos.org/opengl/wiki/OpenGL_Type">Common Enums</a>.
             * GL_FIXED is not supported. Note that every vertex buffers don't support dynamic storage
             * and perform storage access by using a staging buffer.
             */
			VertexBuffer(uint32_t dataType, int32_t stride, size_t elementSize, size_t elementCount,
				void* data = nullptr, uint32_t flags = 0);

			/**
             * Sets the data of a buffer if it isn't mapped.
             * @param data A pointer to the data.
             * @param offset The offset in the buffer in elements (not bytes).
             * @param length The number of elements in data.
             */
			void SetData(void* data, size_t offset, size_t length);

			/**
            * Returns the type of the data which the buffer holds.
            * @return An integer corresponding to an OpenGL data type.
            * @remark For all available data types see https://www.khronos.org/opengl/wiki/OpenGL_Type
            * <a href="https://www.khronos.org/opengl/wiki/OpenGL_Type">Common Enums</a>. GL_FIXED is not supported.
            */
			uint32_t GetDataType();

			/**
            * Returns the stride of each element
            * @return
            * @note Stride is not in bytes but is the number of base elements of type dataType. Have a look at
            * {@link VertexBuffer()} for an example.
            */
			int32_t GetStride();

		private:
			uint32_t dataType = 0;
			int32_t stride = 1;

		};

	}

}

#endif