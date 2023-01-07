#ifndef AE_VERTEXARRAY_H
#define AE_VERTEXARRAY_H

#include "../System.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include "../graphics/CommandList.h"

#include <map>

namespace Atlas {

	namespace Buffer {

		class VertexArray {

		public:
			VertexArray() = default;

			/**
             *
             * @param buffer
             * @note The object takes the ownership of the buffer.
             */
			void AddIndexComponent(IndexBuffer buffer);

			/**
             *
             * @param attribArray
             * @param buffer
             * @param normalized
             * @note The object takes the ownership of the buffer.
             */
			void AddComponent(uint32_t attribArray, VertexBuffer buffer);

			/**
             *
             * @return
             */
			IndexBuffer GetIndexComponent();

            bool HasIndexComponent() const;

			/**
             *
             * @param attribArray
             * @return
             */
			VertexBuffer GetComponent(uint32_t attribArray);

            void Bind(Graphics::CommandList* commandList) const;

            VkPipelineVertexInputStateCreateInfo GetVertexInputState();

		private:
			struct VertexComponent {
                VertexBuffer vertexBuffer;
                VkVertexInputBindingDescription bindingDescription = {};
                VkVertexInputAttributeDescription attributeDescription = {};
            };

			IndexBuffer indexComponent;
			std::map<uint32_t, VertexComponent> vertexComponents;

            bool hasIndexComponent = false;
            std::vector<VkVertexInputBindingDescription> bindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

		};

	}

}

#endif