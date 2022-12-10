#include "PipelineBuilder.h"

namespace Atlas {

    namespace Graphics {

        VkPipeline PipelineBuilder::BuildGraphicsPipeline(MemoryManager* memManager, VkRenderPass renderPass,
            GraphicsPipelineDesc desc) {

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.pNext = nullptr;

            viewportState.viewportCount = 1;
            viewportState.pViewports = &desc.viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &desc.scissor;

            VkPipelineColorBlendStateCreateInfo colorBlending = {};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.pNext = nullptr;

            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &desc.colorBlendAttachment;

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = nullptr;

            pipelineInfo.stageCount = desc.shaderStages.size();
            pipelineInfo.pStages = desc.shaderStages.data();
            pipelineInfo.pVertexInputState = &desc.vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &desc.inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &desc.rasterizer;
            pipelineInfo.pMultisampleState = &desc.multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = desc.pipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

            auto& device = memManager->device;
            VkPipeline pipeline;
            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                    &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
                Log::Error("Failed to create pipeline");
                return VK_NULL_HANDLE;
            }
            else
            {
                return pipeline;
            }

        }

        VkPipeline PipelineBuilder::BuildComputePipeline(MemoryManager* memManager,
            ComputePipelineDesc desc) {

            return VK_NULL_HANDLE;

        }

    }

}