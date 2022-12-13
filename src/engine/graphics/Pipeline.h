#ifndef AE_GRAPHICSPIPELINE_H
#define AE_GRAPHICSPIPELINE_H

#include "Common.h"
#include "MemoryManager.h"
#include "Shader.h"

namespace Atlas {

    namespace Graphics {

        struct GraphicsPipelineDesc {
            // These need to be set
            VkRenderPass renderPass = {};
            Shader* shader = nullptr;

            // These have a valid default state
            VkPipelineVertexInputStateCreateInfo vertexInputInfo =
                Initializers::InitPipelineVertexInputStateCreateInfo();
            VkPipelineInputAssemblyStateCreateInfo assemblyInputInfo =
                Initializers::InitPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            VkPipelineDepthStencilStateCreateInfo depthStencilInputInfo =
                Initializers::InitPipelineDepthStencilStateCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
            VkPipelineRasterizationStateCreateInfo rasterizer =
                Initializers::InitPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
            VkPipelineColorBlendAttachmentState colorBlendAttachment =
                Initializers::InitPipelineColorBlendAttachmentState();
            VkPipelineMultisampleStateCreateInfo multisampling =
                Initializers::InitPipelineMultisampleStateCreateInfo();
        };

        struct ComputePipelineDesc {
            // These need to be set
            Shader* shader = nullptr;
        };

        class Pipeline {
        public:
            Pipeline(MemoryManager* memManager, GraphicsPipelineDesc desc);

            Pipeline(MemoryManager* memManager, ComputePipelineDesc desc);

            ~Pipeline();

            VkPipeline pipeline;
            VkPipelineLayout layout;
            VkPipelineBindPoint bindPoint;

            Shader* shader = nullptr;

            bool isComplete = false;

        private:
            void GeneratePipelineLayoutFromShader(Shader* shader);

            MemoryManager* memoryManager;

        };

    }

}

#endif