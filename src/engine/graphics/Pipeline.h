#ifndef AE_GRAPHICSPIPELINE_H
#define AE_GRAPHICSPIPELINE_H

#include "Common.h"
#include "Shader.h"

#include "../common/Ref.h"

namespace Atlas {

    namespace Graphics {

        struct GraphicsPipelineDesc {
            // These need to be set
            VkRenderPass renderPass = {};
            Ref<Shader> shader = nullptr;

            // These have a valid default state
            VkPipelineVertexInputStateCreateInfo vertexInputInfo =
                Initializers::InitPipelineVertexInputStateCreateInfo();
            VkPipelineInputAssemblyStateCreateInfo assemblyInputInfo =
                Initializers::InitPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            VkPipelineDepthStencilStateCreateInfo depthStencilInputInfo =
                Initializers::InitPipelineDepthStencilStateCreateInfo(true, true, VK_COMPARE_OP_LESS);
            VkPipelineRasterizationStateCreateInfo rasterizer =
                Initializers::InitPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
            VkPipelineColorBlendAttachmentState colorBlendAttachment =
                Initializers::InitPipelineColorBlendAttachmentState();
            VkPipelineMultisampleStateCreateInfo multisampling =
                Initializers::InitPipelineMultisampleStateCreateInfo();
        };

        struct ComputePipelineDesc {
            // These need to be set
            Ref<Shader> shader = nullptr;
        };

        class MemoryManager;

        class Pipeline {
        public:
            Pipeline(MemoryManager* memManager, GraphicsPipelineDesc desc);

            Pipeline(MemoryManager* memManager, ComputePipelineDesc desc);

            ~Pipeline();

            VkPipeline pipeline;
            VkPipelineLayout layout;
            VkPipelineBindPoint bindPoint;

            Ref<Shader> shader = nullptr;

            bool isComplete = false;
            bool isCompute = false;

        private:
            void GeneratePipelineLayoutFromShader(Shader* shader);

            MemoryManager* memoryManager;

        };

    }

}

#endif