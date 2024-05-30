#include "VertexArray.h"

namespace Atlas {

    namespace Buffer {

        void VertexArray::AddIndexComponent(IndexBuffer buffer) {

            hasIndexComponent = true;
            indexComponent = buffer;

        }

        void VertexArray::AddComponent(uint32_t attribArray, VertexBuffer buffer) {

            VertexComponent component;
            component.vertexBuffer = buffer;
            component.attributeDescription = Graphics::Initializers::InitVertexInputAttributeDescription(
                attribArray, buffer.format);
            component.bindingDescription = Graphics::Initializers::InitVertexInputBindingDescription(
                attribArray, buffer.elementSize);

            vertexComponents[attribArray] = component;

            ResetBatches();

        }

        void VertexArray::AddInstancedComponent(uint32_t attribArray, VertexBuffer buffer) {

            VertexComponent component;
            component.vertexBuffer = buffer;
            component.attributeDescription = Graphics::Initializers::InitVertexInputAttributeDescription(
                attribArray, buffer.format);
            component.bindingDescription = Graphics::Initializers::InitVertexInputBindingDescription(
                attribArray, buffer.elementSize);
            component.bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

            vertexComponents[attribArray] = component;

            ResetBatches();

        }

        IndexBuffer VertexArray::GetIndexComponent() {

            return indexComponent;

        }

        bool VertexArray::HasIndexComponent() const {

            return hasIndexComponent;

        }

        VertexBuffer VertexArray::GetComponent(uint32_t attribArray) {

            return vertexComponents[attribArray].vertexBuffer;

        }

        void VertexArray::Bind(Graphics::CommandList *commandList) {

            if (hasIndexComponent) {
                commandList->BindIndexBuffer(indexComponent.buffer, indexComponent.type);
            }

            if (!batches.size())
                BuildBatches();

            for (auto& batch : batches) {
                commandList->BindVertexBuffers(batch.buffers, batch.offset, batch.count);
            }

        }

        VkPipelineVertexInputStateCreateInfo VertexArray::GetVertexInputState() {

            bindingDescriptions.clear();
            attributeDescriptions.clear();

            for (auto& [attribArray, vertexComponent] : vertexComponents) {
                bindingDescriptions.push_back(vertexComponent.bindingDescription);
                attributeDescriptions.push_back(vertexComponent.attributeDescription);
            }

            return Graphics::Initializers::InitPipelineVertexInputStateCreateInfo(
                bindingDescriptions.data(), uint32_t(bindingDescriptions.size()),
                attributeDescriptions.data(), uint32_t(attributeDescriptions.size())
            );

        }

        void VertexArray::ResetBatches() {

            batches.clear();

        }

        void VertexArray::BuildBatches() {

            VertexBufferBatch batch{};

            // Bind in batches to reduce driver binding overhead
            uint32_t bindingOffset = 0;
            uint32_t bindingCount = 0;
            for (auto& [attribArray, vertexComponent] : vertexComponents) {
                if (attribArray != bindingOffset + 1 && bindingCount > 0) {
                    batch.offset = bindingOffset;
                    batch.count = bindingCount;
                    batches.push_back(batch);

                    batch = {};
                    bindingOffset = attribArray;
                    bindingCount = 0;
                }

                batch.buffers.push_back(vertexComponent.vertexBuffer.buffer);
                bindingCount++;
            }

            if (bindingCount) {
                batch.offset = bindingOffset;
                batch.count = bindingCount;
                batches.push_back(batch);
            }

        }

    }

}