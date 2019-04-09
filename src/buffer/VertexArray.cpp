#include "VertexArray.h"

namespace Atlas {

	namespace Buffer {

		uint32_t VertexArray::boundVertexArrayID = 0;

		VertexArray::VertexArray() {

			glGenVertexArrays(1, &ID);

			int32_t maxVertexAttribs;
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

			maxAttribArrayCount = (uint32_t)maxVertexAttribs;

			indexComponent = nullptr;

		}

		void VertexArray::AddIndexComponent(IndexBuffer* buffer) {

			Bind();
			buffer->Bind();

			indexComponent = buffer;

		}

		void VertexArray::AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized) {

			if (attribArray >= maxAttribArrayCount)
				return;

			Bind();
			buffer->Bind();

			glEnableVertexAttribArray(attribArray);
			glVertexAttribPointer(attribArray, buffer->GetStride(), buffer->GetDataType(), normalized, 0, NULL);

			vertexComponents[attribArray] = buffer;

		}

		void VertexArray::AddInstancedComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized) {

			Bind();
			buffer->Bind();

			int32_t bufferStride = buffer->GetStride();

			if(bufferStride > 4) {

				int32_t numAttribArrays = 0;
				int32_t internalStride = 0;

				if (bufferStride % 4 == 0) { // Might be a 4x4 matrix or some other multi-4-component vector data
					numAttribArrays = bufferStride / 4;
					internalStride = 4;
				}
				else if (bufferStride % 3 == 0) { // Might be a 3x3 matrix or some other multi-3-component vector data
					numAttribArrays = bufferStride / 3;
					internalStride = 3;
				}
				else { // Everything else isn't supported. There is no use scenario to justify additional complexity
					return;
				}

				if (attribArray + numAttribArrays > maxAttribArrayCount)
					return;

				for (int32_t i = 0; i < numAttribArrays; i++) {

					auto find = vertexComponents.find(attribArray + i);

					if (find != vertexComponents.end())
						return;

					glEnableVertexAttribArray(attribArray + i);
					glVertexAttribPointer(attribArray + i, internalStride, buffer->GetDataType(), normalized,
							(int32_t)buffer->GetElementSize(), (void*)((uint64_t)(buffer->GetElementSize() / numAttribArrays * i)));
					glVertexAttribDivisor(attribArray + i, 1);

					vertexComponents[attribArray + i] = buffer;

				}

			}
			else {

				if (attribArray >= maxAttribArrayCount)
					return;

				glEnableVertexAttribArray(attribArray);
				glVertexAttribPointer(attribArray, buffer->GetStride(), buffer->GetDataType(), normalized, 0, NULL);
				glVertexAttribDivisor(attribArray, 1);

				vertexComponents[attribArray] = buffer;

			}

		}

		IndexBuffer* VertexArray::GetIndexComponent() {

			return indexComponent;

		}

		VertexBuffer* VertexArray::GetComponent(uint32_t attribArray) {

			return vertexComponents[attribArray];

		}

		void VertexArray::Bind() {

			if (boundVertexArrayID != ID) {

				glBindVertexArray(ID);

				boundVertexArrayID = ID;

			}

		}

		void VertexArray::Unbind() {

			glBindVertexArray(0);

			boundVertexArrayID = 0;

		}

		void VertexArray::ClearContent() {

			indexComponent = nullptr;
			vertexComponents.clear();

		}

		void VertexArray::DeleteContent() {

			delete indexComponent;

			for (auto& bufferKey : vertexComponents) {
				delete bufferKey.second;
			}

			ClearContent();

		}

		VertexArray::~VertexArray() {

			glDeleteVertexArrays(1, &ID);

		}

	}

}