#ifndef AE_INDEXBUFFER_H
#define AE_INDEXBUFFER_H

#include "../System.h"
#include "Buffer.h"

namespace Atlas {

    namespace Buffer {

        class IndexBuffer : public Buffer {

        public:
            /**
             * Constructs an IndexBuffer object.
             * @param dataType The data type of the data, e.g GL_FLOAT
             * @param elementSize The size of each element in bytes
             * @param elementCount The number of elements in the vertex buffer will be filled with
             * @note This is similar to
             * <a href="https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBufferData.xhtml">glBufferData</a>.
             * For all available data types see <a href="https://www.khronos.org/opengl/wiki/OpenGL_Type">Common Enums</a>.
             * GL_FIXED is not supported. Note that every vertex buffers don't support dynamic storage
             * and perform storage access by using a staging buffer.
             */
            IndexBuffer(uint32_t dataType, size_t elementSize, size_t elementCount);

            ~IndexBuffer();

            /**
             * Sets the data of a buffer if it isn't mapped.
             * @param data A pointer to the data.
             * @param offset The offset in the buffer in elements (not bytes).
             * @param length The number of elements in data.
             */
            void SetData(void* data, size_t offset, size_t length) override;

            /**
            * Returns the type of the data which the buffer holds.
            * @return An integer corresponding to an OpenGL data type.
            * @remark For all available data types see https://www.khronos.org/opengl/wiki/OpenGL_Type
            * <a href="https://www.khronos.org/opengl/wiki/OpenGL_Type">Common Enums</a>. GL_FIXED is not supported.
            */
            uint32_t GetDataType();

        private:
            uint32_t dataType;


        };

    }

}

#endif