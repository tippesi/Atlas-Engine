#include "Pipeline.h"

namespace Atlas {

    namespace Graphics {

        Pipeline::Pipeline(MemoryManager *memManager, GraphicsPipelineDesc desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS), shader(desc.shader), memoryManager(memManager) {

            if (desc.shader->isCompute) return;

            GeneratePipelineLayoutFromShader(desc.shader);

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

            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &desc.colorBlendAttachment;

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = nullptr;

            pipelineInfo.stageCount = desc.shader->shaderStageCreateInfos.size();
            pipelineInfo.pStages = desc.shader->shaderStageCreateInfos.data();
            pipelineInfo.pVertexInputState = &desc.vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &desc.assemblyInputInfo;
            pipelineInfo.pDepthStencilState = &desc.depthStencilInputInfo;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &desc.rasterizer;
            pipelineInfo.pMultisampleState = &desc.multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = layout;
            pipelineInfo.renderPass = desc.renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState = &dynamicStateInfo;

            auto& device = memManager->device;
            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
                Log::Error("Failed to create pipeline");
                return;
            }

            isComplete = true;

        }

        Pipeline::Pipeline(MemoryManager *memManager, ComputePipelineDesc desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_COMPUTE), shader(desc.shader), memoryManager(memManager) {

            // TODO..

        }

        Pipeline::~Pipeline() {



        }

        void Pipeline::GeneratePipelineLayoutFromShader(Shader *shader) {

            auto createInfo = Initializers::InitPipelineLayoutCreateInfo();

            std::vector<VkPushConstantRange> pushConstantRanges;
            if (shader->pushConstantRanges.size() > 0) {
                for (auto &range: shader->pushConstantRanges)
                    pushConstantRanges.push_back(range.range);

                createInfo.pushConstantRangeCount = uint32_t(pushConstantRanges.size());
                createInfo.pPushConstantRanges = pushConstantRanges.data();
            }

            VK_CHECK(vkCreatePipelineLayout(memoryManager->device, &createInfo, nullptr, &layout))

        }

    }

}