#include "IndexBuffer.h"

namespace Atlas {

    namespace Buffer {

		IndexBuffer::IndexBuffer(const IndexBuffer& that) : Buffer(that) {

			dataType = that.dataType;

		}

        IndexBuffer::IndexBuffer(uint32_t dataType, size_t elementSize, size_t elementCount,
			void* data, uint32_t flags) : 
			/*Buffer(AE_INDEX_BUFFER, elementSize, flags, elementCount, data),*/ dataType(dataType) {



        }

        IndexBuffer::~IndexBuffer() {



        }

		IndexBuffer& IndexBuffer::operator=(const IndexBuffer& that) {

			if (this != &that) {

				Buffer::operator=(that);

				dataType = that.dataType;

			}

			return *this;

		}

        void IndexBuffer::SetData(void *data, size_t offset, size_t length) {



        }

        uint32_t IndexBuffer::GetDataType() {

            return dataType;

        }

    }

}