#ifndef AE_VERTEXARRAY_H
#define AE_VERTEXARRAY_H

#include "../System.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include <unordered_map>

namespace Atlas {

	namespace Buffer {

		class VertexArray {

		public:

			/**
             *
             */
			VertexArray();

			/**
			 *
			 */
			~VertexArray();

			/**
             *
             * @param buffer
             * @note The object takes the ownership of the buffer.
             */
			void AddIndexComponent(IndexBuffer* buffer);

			/**
             *
             * @param attribArray
             * @param buffer
             * @param normalized
             * @note The object takes the ownership of the buffer.
             */
			void AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

			/**
             *
             * @param attribArray
             * @param buffer
             * @param normalized
             * @note The object takes the ownership of the buffer.
             */
			void AddInstancedComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

			/**
             *
             * @return
             */
			IndexBuffer* GetIndexComponent();

			/**
             *
             * @param attribArray
             * @return
             */
			VertexBuffer* GetComponent(uint32_t attribArray);

			/**
			 * Binds the vertex array.
			 */
			void Bind();

			/**
			 * Unbinds any vertex array.
			 */
			void Unbind();

		private:
			uint32_t ID;

			IndexBuffer* indexComponent;

			std::unordered_map<uint32_t, VertexBuffer*> vertexComponents;

			uint32_t maxAttribArrayCount;

			static uint32_t boundVertexArrayID;

		};

	}

}

#endif