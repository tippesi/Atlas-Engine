#ifndef AE_GRAPHICSPIPELINEBUILDER_H
#define AE_GRAPHICSPIPELINEBUILDER_H

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

            static VkPipeline BuildGraphicsPipeline(MemoryManager* memManager,
                VkRenderPass renderPass, GraphicsPipelineDesc desc);

            static VkPipeline BuildComputePipeline(MemoryManager* memManager,
                ComputePipelineDesc desc);

        };

    }

}

#endif