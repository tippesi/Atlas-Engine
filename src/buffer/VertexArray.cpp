#include "VertexArray.h"

namespace Atlas {

	namespace Buffer {

		VertexArray::VertexArray() {

			glGenVertexArrays(1, &ID);

			int32_t maxVertexAttribs;
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

			maxAttribArrayCount = (uint32_t)maxVertexAttribs;

		}

		VertexArray::~VertexArray() {

			glDeleteVertexArrays(1, &ID);

			delete indexComponent;

			for (auto& key : vertexComponents)
				delete key.second;

		}

		VertexArray& VertexArray::operator=(const VertexArray& that) {

			if (this != &that) {

				Unbind();

				delete indexComponent;

				for (auto& key : vertexComponents)
					delete key.second;
				
				vertexComponents.clear();

				if (that.indexComponent != nullptr) {
					auto buffer = new IndexBuffer(*that.indexComponent);
					AddIndexComponent(buffer);
				}

				for (auto& key : that.vertexComponents) {
					auto buffer = new VertexBuffer(*key.second);
					AddComponent(key.first, buffer);
				}

				Unbind();

			}

			return *this;

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

		void VertexArray::DisableComponent(uint32_t attribArray) {

			Bind();

			glDisableVertexAttribArray(attribArray);

		}

		IndexBuffer* VertexArray::GetIndexComponent() {

			return indexComponent;

		}

		VertexBuffer* VertexArray::GetComponent(uint32_t attribArray) {

			return vertexComponents[attribArray];

		}

		void VertexArray::Bind() const {

			glBindVertexArray(ID);

		}

		void VertexArray::Unbind() const {

			glBindVertexArray(0);

		}

	}

}