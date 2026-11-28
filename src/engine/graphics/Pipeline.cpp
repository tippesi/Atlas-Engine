#include "Pipeline.h"
#include "SwapChain.h"
#include "GraphicsDevice.h"
#include "CommandList.h"

#include <cassert>
#include <cstring>

namespace Atlas {

    namespace Graphics {

        Pipeline::Pipeline(GraphicsDevice* device, const GraphicsPipelineDesc& desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS), shader(desc.shader), device(device) {

            // Shaders which didn't compile leave the pipeline incomplete, users check isComplete
            if (!desc.shader->isComplete) return;

            AE_ASSERT(!desc.shader->isCompute && "Can't create a graphics pipeline with a compute shader");
            if (desc.shader->isCompute) return;

            const auto& formats = desc.attachmentFormats;
            AE_ASSERT((desc.swapChain || formats.colorAttachmentCount || formats.depthFormat) &&
                "Must provide a swap chain or attachment formats");

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

            VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };
            VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
            dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateInfo.dynamicStateCount = 3;
            dynamicStateInfo.pDynamicStates = dynamicStates;

            VkPipelineColorBlendStateCreateInfo colorBlending = {};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.pNext = nullptr;

            // Color blend attachments count must match the number of color attachments
            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
            if (!desc.swapChain) {
                colorBlendAttachments = GenerateBlendAttachmentStates(formats, desc.colorBlendAttachment);
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

            auto assemblyInputInfo = desc.assemblyInputInfo;
#ifdef AE_OS_MACOS
            // Need to disable it for other LIST topologies as well, but they are not in use right now
            if (assemblyInputInfo.topology != VK_PRIMITIVE_TOPOLOGY_LINE_LIST &&
                assemblyInputInfo.topology != VK_PRIMITIVE_TOPOLOGY_POINT_LIST &&
                assemblyInputInfo.topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST &&
                assemblyInputInfo.topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) {
                assemblyInputInfo.primitiveRestartEnable = VK_TRUE;
            }
#endif

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = nullptr;
            pipelineInfo.flags = 0;
            pipelineInfo.stageCount = uint32_t(desc.shader->stageCreateInfos.size());
            pipelineInfo.pStages = desc.shader->stageCreateInfos.data();
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &assemblyInputInfo;
            pipelineInfo.pDepthStencilState = &desc.depthStencilInputInfo;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &desc.rasterizer;
            pipelineInfo.pMultisampleState = &desc.multisampling;
            pipelineInfo.pTessellationState = desc.tessellationInfo.patchControlPoints > 0 ? &desc.tessellationInfo : VK_NULL_HANDLE;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = layout;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = 0;
            pipelineInfo.pDynamicState = &dynamicStateInfo;

            // Passes use dynamic rendering, only the swap chain still has a render pass
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            if (!desc.swapChain) {
                renderingInfo.colorAttachmentCount = formats.colorAttachmentCount;
                renderingInfo.pColorAttachmentFormats = formats.colorFormats;
                renderingInfo.depthAttachmentFormat = formats.depthFormat;

                pipelineInfo.pNext = &renderingInfo;
            }
            else {
                pipelineInfo.renderPass = desc.swapChain->renderPass;
            }

            auto result = vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1,
                &pipelineInfo, nullptr, &pipeline);
            // Some drivers report success without a pipeline when their shader translation failed
            if (result != VK_SUCCESS || pipeline == VK_NULL_HANDLE) {
                Log::Error("Failed to create graphics pipeline: " + VkResultToString(result));
                return;
            }

            isComplete = true;
            isCompute = false;

        }

        Pipeline::Pipeline(GraphicsDevice* device, const ComputePipelineDesc& desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_COMPUTE), shader(desc.shader), device(device) {

            if (!desc.shader->isComplete) return;

            AE_ASSERT(desc.shader->isCompute && "Can't create a compute pipeline without a compute shader");
            if (!desc.shader->isCompute) return;

            GeneratePipelineLayoutFromShader();

            VkComputePipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = nullptr;

            pipelineInfo.flags = 0;
            pipelineInfo.stage = shader->stageCreateInfos.front();
            VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroupSizeInfo = {};
            if (desc.requiredSubgroupSize != 0) {
                subgroupSizeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO;
                subgroupSizeInfo.requiredSubgroupSize = desc.requiredSubgroupSize;
                pipelineInfo.stage.pNext = &subgroupSizeInfo;
            }
            pipelineInfo.layout = layout;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = 0;

            auto result = vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1,
                &pipelineInfo, nullptr, &pipeline);
            if (result != VK_SUCCESS || pipeline == VK_NULL_HANDLE) {
                Log::Error("Failed to create compute pipeline: " + VkResultToString(result));
                return;
            }

            isComplete = true;
            isCompute = true;

        }

        Pipeline::Pipeline(GraphicsDevice* device, const RayTracingPipelineDesc& desc) :
            bindPoint(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR), shader(desc.shader), device(device) {

            if (!desc.shader->isComplete) return;

            GeneratePipelineLayoutFromShader();

            const auto& stages = shader->stageCreateInfos;

            auto createGroup = [](VkRayTracingShaderGroupTypeKHR type) {
                VkRayTracingShaderGroupCreateInfoKHR group = {};
                group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                group.type = type;
                group.generalShader = VK_SHADER_UNUSED_KHR;
                group.closestHitShader = VK_SHADER_UNUSED_KHR;
                group.anyHitShader = VK_SHADER_UNUSED_KHR;
                group.intersectionShader = VK_SHADER_UNUSED_KHR;
                return group;
            };

            // Groups are ordered like the shader binding table: ray generation, miss, hit, callable. The first hit group
            // only has the first any hit stage, closest hit stage i gets a group with any hit stage i + 1 after it
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
            auto addGeneralGroups = [&](VkShaderStageFlagBits stage) {
                for (uint32_t i = 0; i < uint32_t(stages.size()); i++) {
                    if (stages[i].stage != stage) continue;

                    auto group = createGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);
                    group.generalShader = i;
                    groups.push_back(group);
                }
            };

            addGeneralGroups(VK_SHADER_STAGE_RAYGEN_BIT_KHR);
            auto rayGenCount = uint32_t(groups.size());
            addGeneralGroups(VK_SHADER_STAGE_MISS_BIT_KHR);
            auto missCount = uint32_t(groups.size()) - rayGenCount;

            std::vector<uint32_t> anyHitStages, closestHitStages;
            for (uint32_t i = 0; i < uint32_t(stages.size()); i++) {
                if (stages[i].stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
                    anyHitStages.push_back(i);
                if (stages[i].stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
                    closestHitStages.push_back(i);
            }

            auto hitGroup = createGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR);
            if (!anyHitStages.empty())
                hitGroup.anyHitShader = anyHitStages.front();
            if (!anyHitStages.empty() || !closestHitStages.empty())
                groups.push_back(hitGroup);
            for (size_t i = 0; i < closestHitStages.size(); i++) {
                // Instances select these groups while traversing as well
                auto closestHitGroup = createGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR);
                closestHitGroup.closestHitShader = closestHitStages[i];
                closestHitGroup.anyHitShader = i + 1 < anyHitStages.size() ? anyHitStages[i + 1] : hitGroup.anyHitShader;
                groups.push_back(closestHitGroup);
            }
            auto hitCount = uint32_t(groups.size()) - rayGenCount - missCount;
            addGeneralGroups(VK_SHADER_STAGE_CALLABLE_BIT_KHR);
            auto callableCount = uint32_t(groups.size()) - rayGenCount - missCount - hitCount;

            AE_ASSERT(rayGenCount == 1 && "A ray tracing pipeline needs exactly one ray generation stage");
            if (rayGenCount != 1) return;

            VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
            pipelineInfo.stageCount = uint32_t(stages.size());
            pipelineInfo.pStages = stages.data();
            pipelineInfo.groupCount = uint32_t(groups.size());
            pipelineInfo.pGroups = groups.data();
            // Rays are only traced from the ray generation stage
            pipelineInfo.maxPipelineRayRecursionDepth = 1;
            pipelineInfo.layout = layout;

            auto result = vkCreateRayTracingPipelinesKHR(device->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
                &pipelineInfo, nullptr, &pipeline);
            if (result != VK_SUCCESS) {
                Log::Error("Failed to create ray tracing pipeline: " + VkResultToString(result));
                return;
            }

            missGroupCount = missCount;
            hitGroupCount = hitCount;
            callableGroupCount = callableCount;

            auto handleSize = device->rayTracingPipelineProperties.shaderGroupHandleSize;
            groupHandles.resize(size_t(groups.size()) * handleSize);
            VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device->device, pipeline, 0, uint32_t(groups.size()),
                groupHandles.size(), groupHandles.data()))

            isComplete = true;

        }

        Pipeline::~Pipeline() {

            // The layout is created before the pipeline, a failed pipeline creation still owns it
            if (pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device->device, pipeline, nullptr);
            if (layout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(device->device, layout, nullptr);

        }

        ShaderBindingTable Pipeline::CreateShaderBindingTable(const std::vector<uint32_t>& hitRecords,
            CommandList* commandList) const {

            const auto& properties = device->rayTracingPipelineProperties;
            auto alignUp = [](uint32_t value, uint32_t alignment) { return (value + alignment - 1) & ~(alignment - 1); };

            auto handleSize = properties.shaderGroupHandleSize;
            auto handleStride = alignUp(handleSize, properties.shaderGroupHandleAlignment);
            auto baseAlignment = properties.shaderGroupBaseAlignment;

            auto hitRecordCount = uint32_t(hitRecords.size());

            // Each region starts at the base alignment, the ray generation region holds exactly one record
            uint32_t rayGenSize = alignUp(handleStride, baseAlignment);
            uint32_t missSize = alignUp(missGroupCount * handleStride, baseAlignment);
            uint32_t hitSize = alignUp(hitRecordCount * handleStride, baseAlignment);
            uint32_t callableSize = alignUp(callableGroupCount * handleStride, baseAlignment);

            std::vector<uint8_t> table(size_t(rayGenSize) + missSize + hitSize + callableSize, 0);
            auto copyHandle = [&](uint32_t group, uint32_t offset) {
                std::memcpy(table.data() + offset, groupHandles.data() + size_t(group) * handleSize, handleSize);
            };
            copyHandle(0, 0);
            for (uint32_t i = 0; i < missGroupCount; i++)
                copyHandle(1 + i, rayGenSize + i * handleStride);
            for (uint32_t i = 0; i < hitRecordCount; i++) {
                auto group = hitRecords[i] < hitGroupCount ? hitRecords[i] : 0;
                copyHandle(1 + missGroupCount + group, rayGenSize + missSize + i * handleStride);
            }
            for (uint32_t i = 0; i < callableGroupCount; i++)
                copyHandle(1 + missGroupCount + hitGroupCount + i, rayGenSize + missSize + hitSize + i * handleStride);

            BufferDesc desc = {
                .usageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .domain = BufferDomain::Device,
                .size = table.size(),
                .alignment = baseAlignment
            };

            ShaderBindingTable shaderBindingTable;
            shaderBindingTable.buffer = device->CreateBuffer(desc);

            BufferDesc stagingDesc = {
                .usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .domain = BufferDomain::Host,
                .data = table.data(),
                .size = table.size()
            };
            auto stagingBuffer = device->CreateBuffer(stagingDesc);
            commandList->CopyBuffer(stagingBuffer, shaderBindingTable.buffer);
            commandList->MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

            auto address = shaderBindingTable.buffer->GetDeviceAddress();
            shaderBindingTable.rayGenRegion = { address, rayGenSize, rayGenSize };
            if (missGroupCount > 0)
                shaderBindingTable.missRegion = { address + rayGenSize, handleStride, missGroupCount * handleStride };
            if (hitRecordCount > 0)
                shaderBindingTable.hitRegion = { address + rayGenSize + missSize, handleStride, hitRecordCount * handleStride };
            if (callableGroupCount > 0)
                shaderBindingTable.callableRegion = { address + rayGenSize + missSize + hitSize, handleStride,
                    callableGroupCount * handleStride };

            return shaderBindingTable;

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
                //if (!shader->sets[i].) continue;
                descriptorSetLayouts.push_back(shader->sets[i].layout->layout);
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

            AE_ASSERT(descVertexInputState.vertexBindingDescriptionCount ==
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
                AE_ASSERT(found && "Vertex input was not specified in the pipeline desc");
            }

            return Graphics::Initializers::InitPipelineVertexInputStateCreateInfo(
                bindingDescriptions.data(), uint32_t(bindingDescriptions.size()),
                attributeDescriptions.data(), uint32_t(attributeDescriptions.size())
            );

        }

        std::vector<VkPipelineColorBlendAttachmentState> Pipeline::GenerateBlendAttachmentStates(
            const AttachmentFormats& attachmentFormats, VkPipelineColorBlendAttachmentState blendAttachmentState) {

            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

            for (uint32_t i = 0; i < attachmentFormats.colorAttachmentCount; i++) {
                auto blendAttachment = blendAttachmentState;
                if (!attachmentFormats.colorWriteEnabled[i])
                    blendAttachment.colorWriteMask = 0;
                colorBlendAttachments.push_back(blendAttachment);
            }

            return colorBlendAttachments;

        }

    }

}
