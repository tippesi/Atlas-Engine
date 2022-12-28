#ifndef AE_GRAPHICSPIPELINE_H
#define AE_GRAPHICSPIPELINE_H

#include "Common.h"
#include "Shader.h"
#include "Framebuffer.h"

#include "../common/Ref.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class SwapChain;

        struct GraphicsPipelineDesc {
            // These need to be set
            // If renderPass isn't set, we need the swap chain
            SwapChain* swapChain = nullptr;
            Ref<FrameBuffer> frameBuffer = nullptr;
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

        class Pipeline {
        public:
            Pipeline(GraphicsDevice* memManager, GraphicsPipelineDesc desc);

            Pipeline(GraphicsDevice* memManager, ComputePipelineDesc desc);

            ~Pipeline();

            VkPipeline pipeline;
            VkPipelineLayout layout;
            VkPipelineBindPoint bindPoint;

            Ref<Shader> shader = nullptr;
            Ref<FrameBuffer> frameBuffer = nullptr;

            bool isComplete = false;
            bool isCompute = false;

        private:
            void GeneratePipelineLayoutFromShader();

            std::vector<VkPipelineColorBlendAttachmentState> GenerateBlendAttachmentStateFromFrameBuffer(
                VkPipelineColorBlendAttachmentState blendAttachmentState);

            GraphicsDevice* device;

        };

    }

}

#endif