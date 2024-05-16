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

            // Bind in batches to reduce driver binding overhead
            uint32_t bindingOffset = 0;
            if (bindingBuffers.size() < vertexComponents.size())
                bindingBuffers.resize(vertexComponents.size());

            uint32_t bindingCount = 0;
            for (auto& [attribArray, vertexComponent] : vertexComponents) {
                if (attribArray != bindingOffset + 1 && bindingBuffers.size() > 0) {
                    commandList->BindVertexBuffers(bindingBuffers, bindingOffset, bindingCount);

                    bindingOffset = attribArray;
                    bindingCount = 0;
                }

                bindingBuffers[bindingCount++] = vertexComponent.vertexBuffer.buffer;
            }

            if (bindingBuffers.size()) {
                commandList->BindVertexBuffers(bindingBuffers, bindingOffset, bindingCount);
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

    }

}