#pragma once

#include "../System.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include "../graphics/CommandList.h"
#include "../graphics/Buffer.h"

#include <map>

namespace Atlas {

    namespace Buffer {

        class VertexArray {

        public:
            VertexArray() = default;

            /**
             *
             * @param buffer
             */
            void AddIndexComponent(IndexBuffer buffer);

            /**
             *
             * @param attribArray
             * @param buffer
             * @param normalized
             */
            void AddComponent(uint32_t attribArray, VertexBuffer buffer);

            /**
             *
             * @param attribArray
             * @param buffer
             */
            void AddInstancedComponent(uint32_t attribArray, VertexBuffer buffer);

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

            // Without the offsets of the buffers, draws select the vertices and indices in them (geometry pool)
            void Bind(Graphics::CommandList* commandList, bool withOffsets = true);

            VkPipelineVertexInputStateCreateInfo GetVertexInputState();

        private:
            void ResetBatches();

            void BuildBatches();

            struct VertexComponent {
                VertexBuffer vertexBuffer;
                VkVertexInputBindingDescription bindingDescription = {};
                VkVertexInputAttributeDescription attributeDescription = {};
            };

            struct VertexBufferBatch {
                uint32_t offset;
                uint32_t count;
                std::vector<Ref<Graphics::Buffer>> buffers;
                std::vector<VkDeviceSize> bufferOffsets;
                std::vector<VkDeviceSize> zeroOffsets;
            };

            IndexBuffer indexComponent;
            std::map<uint32_t, VertexComponent> vertexComponents;

            bool hasIndexComponent = false;
            std::vector<VkVertexInputBindingDescription> bindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

            std::vector<VertexBufferBatch> batches;

        };

    }

}