#include "Shader.h"
#include "Extensions.h"
#include "ShaderCompiler.h"
#include "PipelineBuilder.h"

#include <cassert>

namespace Atlas {

    namespace Graphics {

        const std::string ShaderStageFile::GetGlslCode() const {

            std::string glslCode = "";
            glslCode.append("#version 460\n\n");

            if (Extensions::IsSupported("GL_EXT_texture_shadow_lod")) {
                glslCode.append("#define AE_TEXTURE_SHADOW_LOD\n");
            }

            // Extensions have to come first
            for (auto& extension : extensions) {
                for (auto& ifdef : extension.ifdefs)
                    glslCode += ifdef + "\n";
                glslCode += extension.extension + "\n";
                for (size_t i = 0; i < extension.ifdefs.size(); i++)
                    glslCode += "#endif\n";
            }

            for (auto& macro : macros) {
                glslCode.append("#define " + macro + "\n");
            }

            glslCode.append(code);
            glslCode.erase(std::remove(glslCode.begin(), glslCode.end(), '\r'), glslCode.end());

            return glslCode;

        }

        Shader::Shader(MemoryManager *memManager, ShaderDesc &desc) : memoryManager(memManager) {

            shaderModules.resize(desc.stages.size());
            std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
            for (size_t i = 0; i < desc.stages.size(); i++) {
                auto& stage = desc.stages[i];

                if (!stage.isCompiled) {
                    ShaderCompiler::Compile(stage);
                }

                if (!stage.isCompiled) return;

                VkShaderModuleCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.pNext = nullptr;
                createInfo.codeSize = stage.spirvBinary.size() * sizeof(uint32_t);
                createInfo.pCode = stage.spirvBinary.data();

                bool success = vkCreateShaderModule(memManager->device, &createInfo,
                    nullptr, &shaderModules[i]) == VK_SUCCESS;
                assert(success && "Error creating shader module");
                if (!success) {
                    return;
                }

                // Compute shaders need a different pipeline creation
                if (stage.shaderStage != VK_SHADER_STAGE_COMPUTE_BIT)
                    isCompute = false;

                shaderStageCreateInfos.push_back(Initializers::InitPipelineShaderStageCreateInfo(
                    stage.shaderStage, shaderModules[i]
                    ));

            }

            if (!isCompute) {

                auto layoutPipelineCreateInfo = Initializers::InitPipelineLayoutCreateInfo();
                VK_CHECK(vkCreatePipelineLayout(memManager->device, &layoutPipelineCreateInfo, nullptr, &pipelineLayout))

                PipelineBuilder::GraphicsPipelineDesc graphicsPipelineDesc;
                graphicsPipelineDesc = PipelineBuilder::GraphicsPipelineDesc{
                    .shaderStages = shaderStageCreateInfos,
                    .vertexInputInfo = Initializers::InitPipelineVertexInputStateCreateInfo(),
                    .inputAssembly = Initializers::InitPipelineInputAssemblyStateCreateInfo(
                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
                    .rasterizer = Initializers::InitPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL),
                    .colorBlendAttachment = Initializers::InitPipelineColorBlendAttachmentState(),
                    .multisampling = Initializers::InitPipelineMultisampleStateCreateInfo(),
                    .pipelineLayout = pipelineLayout
                };

                graphicsPipelineDesc.viewport.x = 0.0f;
                graphicsPipelineDesc.viewport.y = 0.0f;
                graphicsPipelineDesc.viewport.width = (float)desc.viewportWidth;
                graphicsPipelineDesc.viewport.height = (float)desc.viewportHeight;
                graphicsPipelineDesc.viewport.minDepth = 0.0f;
                graphicsPipelineDesc.viewport.maxDepth = 1.0f;

                graphicsPipelineDesc.scissor.offset = { 0, 0 };
                graphicsPipelineDesc.scissor.extent = { desc.viewportWidth, desc.viewportHeight } ;

                pipeline = PipelineBuilder::BuildGraphicsPipeline(memManager, desc.renderPass,
                    graphicsPipelineDesc);

            }
            else {

            }

            isComplete = true;

        }

        Shader::~Shader() {



        }



    }

}