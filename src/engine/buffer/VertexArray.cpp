#include "VertexArray.h"

namespace Atlas {

	namespace Buffer {

		VertexArray::VertexArray() {



		}

		VertexArray::~VertexArray() {

            // glDeleteVertexArrays(1, &ID);

			delete indexComponent;

			// Instanced buffers are in multiple slots
			std::unordered_set<VertexBuffer*> buffers;

			for (auto& key : vertexComponents) {
				buffers.insert(key.second);
			}

			for (auto buffer : buffers) {
				delete buffer;
			}
				
		}

		void VertexArray::AddIndexComponent(IndexBuffer* buffer) {

			indexComponent = buffer;

		}

		void VertexArray::AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized) {

			if (attribArray >= maxAttribArrayCount)
				return;

            // glEnableVertexAttribArray(attribArray);
            // glVertexAttribPointer(attribArray, buffer->GetStride(), buffer->GetDataType(), normalized, 0, nullptr);

			vertexComponents[attribArray] = buffer;

		}

		void VertexArray::DisableComponent(uint32_t attribArray) {

            // glDisableVertexAttribArray(attribArray);

		}

		IndexBuffer* VertexArray::GetIndexComponent() {

			return indexComponent;

		}

		VertexBuffer* VertexArray::GetComponent(uint32_t attribArray) {

			return vertexComponents[attribArray];

		}

		void VertexArray::UpdateComponents() {

			if (indexComponent) {
				AddIndexComponent(indexComponent);
			}

			// Instanced buffers are in multiple slots
			std::unordered_set<VertexBuffer*> buffers;

			for (auto& key : vertexComponents) {
				if (buffers.find(key.second) == buffers.end()) {
					AddComponent(key.first, key.second);
					buffers.insert(key.second);
				}
			}

		}

	}

}