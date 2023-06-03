#include "Pipeline.h"
#include "SwapChain.h"
#include "GraphicsDevice.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        Pipeline::Pipeline(GraphicsDevice* device, const GraphicsPipelineDesc& desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS), shader(desc.shader),
            frameBuffer(desc.frameBuffer), device(device) {

            assert(!desc.shader->isCompute && "Can't create a graphics pipeline with a compute shader");
            if (desc.shader->isCompute) return;

            assert((desc.swapChain || desc.frameBuffer) && "Must provide a swap chain or a frame buffer");

            GeneratePipelineLayoutFromShader();

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.pNext = nullptr;

            // Viewport and scissor we be handled as dynamic states
            VkViewport viewport = {};
            VkRect2D scissor = {};
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
            dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateInfo.dynamicStateCount = 2;
            dynamicStateInfo.pDynamicStates = dynamicStates;

            VkPipelineColorBlendStateCreateInfo colorBlending = {};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.pNext = nullptr;

            // Color blend attachments count must match the number of color attachments
            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
            if (frameBuffer != nullptr) {
                colorBlendAttachments = GenerateBlendAttachmentStateFromFrameBuffer(desc.colorBlendAttachment);
            }
            else {
                colorBlendAttachments.push_back(desc.colorBlendAttachment);
            }

            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = uint32_t(colorBlendAttachments.size());
            colorBlending.pAttachments = colorBlendAttachments.data();

            std::vector<VkVertexInputBindingDescription> bindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
            auto vertexInputInfo = GenerateVertexInputStateInfoFromShader(desc.vertexInputInfo,
                bindingDescriptions, attributeDescriptions);

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = nullptr;
            pipelineInfo.flags = 0;
            pipelineInfo.stageCount = uint32_t(desc.shader->stageCreateInfos.size());
            pipelineInfo.pStages = desc.shader->stageCreateInfos.data();
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &desc.assemblyInputInfo;
            pipelineInfo.pDepthStencilState = &desc.depthStencilInputInfo;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &desc.rasterizer;
            pipelineInfo.pMultisampleState = &desc.multisampling;
            pipelineInfo.pTessellationState = desc.tessellationInfo.patchControlPoints > 0 ? &desc.tessellationInfo : VK_NULL_HANDLE;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = layout;
            pipelineInfo.renderPass = frameBuffer ? frameBuffer->renderPass->renderPass : desc.swapChain->renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = 0;
            pipelineInfo.pDynamicState = &dynamicStateInfo;

            auto result = vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1,
                &pipelineInfo, nullptr, &pipeline);
            if (result != VK_SUCCESS) {
                Log::Error("Failed to create graphics pipeline: " + VkResultToString(result));
                return;
            }

            isComplete = true;
            isCompute = false;

        }

        Pipeline::Pipeline(GraphicsDevice* device, const ComputePipelineDesc& desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_COMPUTE), shader(desc.shader), device(device) {

            assert(desc.shader->isCompute && "Can't create a compute pipeline without a compute shader");
            if (!desc.shader->isCompute) return;

            GeneratePipelineLayoutFromShader();

            VkComputePipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = nullptr;

            pipelineInfo.flags = 0;
            pipelineInfo.stage = shader->stageCreateInfos.front();
            pipelineInfo.layout = layout;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = 0;

            auto result = vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1,
                &pipelineInfo, nullptr, &pipeline);
            if (result != VK_SUCCESS) {
                Log::Error("Failed to create compute pipeline: " + VkResultToString(result));
                return;
            }

            isComplete = true;
            isCompute = true;

        }

        Pipeline::~Pipeline() {

            if (!isComplete) return;

            vkDestroyPipeline(device->device, pipeline, nullptr);
            vkDestroyPipelineLayout(device->device, layout, nullptr);

        }

        void Pipeline::GeneratePipelineLayoutFromShader() {

            auto createInfo = Initializers::InitPipelineLayoutCreateInfo();

            std::vector<VkPushConstantRange> pushConstantRanges;
            if (shader->pushConstantRanges.size() > 0) {
                for (auto &range: shader->pushConstantRanges)
                    pushConstantRanges.push_back(range.range);

                createInfo.pushConstantRangeCount = uint32_t(pushConstantRanges.size());
                createInfo.pPushConstantRanges = pushConstantRanges.data();
            }

            std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                // We need to check if there are any bindings at all
                //if (!shader->sets[i].bindingCount) continue;
                descriptorSetLayouts.push_back(shader->sets[i].layout);
            }

            if (descriptorSetLayouts.size() > 0) {
                createInfo.setLayoutCount = uint32_t(descriptorSetLayouts.size());
                createInfo.pSetLayouts = descriptorSetLayouts.data();
            }

            VK_CHECK(vkCreatePipelineLayout(device->device, &createInfo, nullptr, &layout))

        }

        VkPipelineVertexInputStateCreateInfo Pipeline::GenerateVertexInputStateInfoFromShader(
            VkPipelineVertexInputStateCreateInfo descVertexInputState,
            std::vector<VkVertexInputBindingDescription>& bindingDescriptions,
            std::vector<VkVertexInputAttributeDescription>& attributeDescriptions) {

            assert(descVertexInputState.vertexBindingDescriptionCount ==
                descVertexInputState.vertexAttributeDescriptionCount && "Expected bindings and attributes \
                to have the same amount of elements");

            for (auto& vertexInput : shader->vertexInputs) {
                bool found = false;
                for (uint32_t i = 0; i < descVertexInputState.vertexBindingDescriptionCount; i++) {
                    auto vertexBinding = descVertexInputState.pVertexBindingDescriptions[i];
                    auto vertexAttribute = descVertexInputState.pVertexAttributeDescriptions[i];
                    if (vertexBinding.binding == vertexInput.location) {
                        bindingDescriptions.push_back(vertexBinding);
                        attributeDescriptions.push_back(vertexAttribute);
                        found = true;
                        break;
                    }
                }
                assert(found && "Vertex input was not specified in the pipeline desc");
            }

            return Graphics::Initializers::InitPipelineVertexInputStateCreateInfo(
                bindingDescriptions.data(), uint32_t(bindingDescriptions.size()),
                attributeDescriptions.data(), uint32_t(attributeDescriptions.size())
            );

        }

        std::vector<VkPipelineColorBlendAttachmentState> Pipeline::GenerateBlendAttachmentStateFromFrameBuffer(
            VkPipelineColorBlendAttachmentState blendAttachmentState) {

            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

            auto& renderPass = frameBuffer->renderPass;
            for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
                auto& rpColorAttachment = renderPass->colorAttachments[i];
                auto& fbColorAttachment = frameBuffer->colorAttachments[i];

                // No valid attachment in the render pass, continue
                if (!rpColorAttachment.isValid) continue;

                auto blendAttachment = blendAttachmentState;
                // Disable writing to attachments that are not in the frame buffer
                if (!fbColorAttachment.writeEnabled) {
                    blendAttachment.colorWriteMask = 0;
                }
                colorBlendAttachments.push_back(blendAttachment);
            }

            return colorBlendAttachments;

        }

    }

}