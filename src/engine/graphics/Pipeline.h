#pragma once

#include "Common.h"
#include "Shader.h"
#include "Rendering.h"
#include "Buffer.h"

#include "../common/Ref.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;
        class SwapChain;

        struct GraphicsPipelineDesc {
            // These need to be set, the attachment formats are only used without a swap chain
            SwapChain* swapChain = nullptr;
            AttachmentFormats attachmentFormats;
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
            VkPipelineTessellationStateCreateInfo tessellationInfo =
                Initializers::InitPipelineTessellationStateCreateInfo(0);

        };

        struct ComputePipelineDesc {
            // These need to be set
            Ref<ShaderVariant> shader = nullptr;
            // Zero lets the implementation choose the subgroup size.
            uint32_t requiredSubgroupSize = 0;
        };

        // One ray generation stage and optional miss, any hit, closest hit and callable stages. The first hit group
        // only has the first any hit stage, closest hit stage i gets a group with any hit stage i + 1 after that one
        struct RayTracingPipelineDesc {
            Ref<ShaderVariant> shader = nullptr;
        };

        class CommandList;

        struct ShaderBindingTable {
            Ref<Buffer> buffer;
            VkStridedDeviceAddressRegionKHR rayGenRegion = {};
            VkStridedDeviceAddressRegionKHR missRegion = {};
            VkStridedDeviceAddressRegionKHR hitRegion = {};
            VkStridedDeviceAddressRegionKHR callableRegion = {};
        };

        class Pipeline {
        public:
            Pipeline(GraphicsDevice* device, const GraphicsPipelineDesc& desc);

            Pipeline(GraphicsDevice* device, const ComputePipelineDesc& desc);

            Pipeline(GraphicsDevice* device, const RayTracingPipelineDesc& desc);

            ~Pipeline();

            VkPipeline pipeline = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            VkPipelineBindPoint bindPoint;

            Ref<ShaderVariant> shader = nullptr;

            bool isComplete = false;
            bool isCompute = false;

            // A hit record per entry with the hit group of that index, missing groups get the first one. The table is
            // copied in the command list
            ShaderBindingTable CreateShaderBindingTable(const std::vector<uint32_t>& hitRecords,
                CommandList* commandList) const;

        private:
            void GeneratePipelineLayoutFromShader();

            std::vector<uint8_t> groupHandles;
            uint32_t missGroupCount = 0;
            uint32_t hitGroupCount = 0;
            uint32_t callableGroupCount = 0;

            VkPipelineVertexInputStateCreateInfo GenerateVertexInputStateInfoFromShader(
                VkPipelineVertexInputStateCreateInfo descVertexInputState,
                std::vector<VkVertexInputBindingDescription>& bindingDescriptions,
                std::vector<VkVertexInputAttributeDescription>& attributeDescriptions);

            std::vector<VkPipelineColorBlendAttachmentState> GenerateBlendAttachmentStates(
                const AttachmentFormats& attachmentFormats, VkPipelineColorBlendAttachmentState blendAttachmentState);

            GraphicsDevice* device;

        };

    }

}
