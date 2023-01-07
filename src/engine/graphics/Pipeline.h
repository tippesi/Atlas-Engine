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
            Ref<ShaderVariant> shader = nullptr;

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
            Ref<ShaderVariant> shader = nullptr;
        };

        class Pipeline {
        public:
            Pipeline(GraphicsDevice* memManager, GraphicsPipelineDesc desc);

            Pipeline(GraphicsDevice* memManager, ComputePipelineDesc desc);

            ~Pipeline();

            VkPipeline pipeline;
            VkPipelineLayout layout;
            VkPipelineBindPoint bindPoint;

            Ref<ShaderVariant> shader = nullptr;
            Ref<FrameBuffer> frameBuffer = nullptr;

            bool isComplete = false;
            bool isCompute = false;

        private:
            void GeneratePipelineLayoutFromShader();

            VkPipelineVertexInputStateCreateInfo GenerateVertexInputStateInfoFromShader(
                VkPipelineVertexInputStateCreateInfo descVertexInputState,
                std::vector<VkVertexInputBindingDescription>& bindingDescriptions,
                std::vector<VkVertexInputAttributeDescription>& attributeDescriptions);

            std::vector<VkPipelineColorBlendAttachmentState> GenerateBlendAttachmentStateFromFrameBuffer(
                VkPipelineColorBlendAttachmentState blendAttachmentState);

            GraphicsDevice* device;

        };

    }

}

#endif