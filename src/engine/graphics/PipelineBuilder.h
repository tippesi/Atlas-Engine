#ifndef AE_PIPELINEBUILDER_H
#define AE_PIPELINEBUILDER_H

#include "Common.h"
#include "MemoryManager.h"

namespace Atlas {

    namespace Graphics {

        class PipelineBuilder {

        public:
            struct GraphicsPipelineDesc {
                std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
                VkPipelineVertexInputStateCreateInfo vertexInputInfo;
                VkPipelineInputAssemblyStateCreateInfo inputAssembly;
                VkViewport viewport;
                VkRect2D scissor;
                VkPipelineRasterizationStateCreateInfo rasterizer;
                VkPipelineColorBlendAttachmentState colorBlendAttachment;
                VkPipelineMultisampleStateCreateInfo multisampling;
                VkPipelineLayout pipelineLayout;
            };

            struct ComputePipelineDesc {

            };

            PipelineBuilder() = default;

            VkPipeline BuildGraphicsPipeline(MemoryManager* memManager, VkRenderPass renderPass, GraphicsPipelineDesc desc);

            VkPipeline BuildComputePipeline(MemoryManager* memManager, ComputePipelineDesc desc);

        };

    }

}

#endif