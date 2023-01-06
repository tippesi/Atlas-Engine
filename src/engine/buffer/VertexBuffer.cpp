#include "VertexBuffer.h"

namespace Atlas {

	namespace Buffer {


		VertexBuffer::VertexBuffer(uint32_t dataType, int32_t stride, size_t elementSize,
			size_t elementCount, void* data, uint32_t flags) :
			dataType(dataType), stride(stride) {



		}

		void VertexBuffer::SetData(void *data, size_t offset, size_t length) {



		}

		uint32_t VertexBuffer::GetDataType() {

			return dataType;

		}

		int32_t VertexBuffer::GetStride() {

			return stride;

		}

	}

}