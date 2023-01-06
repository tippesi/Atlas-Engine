#include "IndexBuffer.h"

namespace Atlas {

    namespace Buffer {

        IndexBuffer::IndexBuffer(uint32_t dataType, size_t elementSize, size_t elementCount,
			void* data, uint32_t flags) : 
			/*Buffer(AE_INDEX_BUFFER, elementSize, flags, elementCount, data),*/ dataType(dataType) {



        }

        void IndexBuffer::SetData(void *data, size_t offset, size_t length) {



        }

        uint32_t IndexBuffer::GetDataType() {

            return dataType;

        }

    }

}