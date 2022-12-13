#include "Shader.h"
#include "Extensions.h"
#include "ShaderCompiler.h"

#include <spirv_reflect.h>
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

            uint32_t allStageFlags = 0;

            shaderModules.resize(desc.stages.size());
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
                    nullptr, &shaderModules[i].module) == VK_SUCCESS;
                assert(success && "Error creating shader module");
                if (!success) {
                    return;
                }

                // Compute shaders need a different pipeline creation
                if (stage.shaderStage != VK_SHADER_STAGE_COMPUTE_BIT)
                    isCompute = false;

                shaderStageCreateInfos.push_back(Initializers::InitPipelineShaderStageCreateInfo(
                    stage.shaderStage, shaderModules[i].module
                    ));

                GenerateReflectionData(shaderModules[i], stage);

                shaderModules[i].shaderStageFlag = stage.shaderStage;
                allStageFlags |= shaderModules[i].shaderStageFlag;

            }

            // For now just take the reflection data of the first module
            // In this case we assume each stage receives the same data
            pushConstantRanges = shaderModules.front().pushConstantRanges;
            // Need to have the flags for all stages used by this shader
            for (auto& pushRange : pushConstantRanges)
                pushRange.range.stageFlags |= allStageFlags;

            isComplete = true;

        }

        Shader::~Shader() {



        }

        PushConstantRange* Shader::GetPushConstantRange(const std::string &name) {

            auto it = std::find_if(pushConstantRanges.begin(), pushConstantRanges.end(),
                [name](const auto& pushConstantRange) { return pushConstantRange.name == name; });

            if (it != pushConstantRanges.end())
                return &(*it);
            else
                assert(0 && "Couldn't find the requested push constant range");

            return nullptr;

        }

        void Shader::GenerateReflectionData(ShaderModule &shaderModule, ShaderStageFile& shaderStageFile) {

            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(shaderStageFile.spirvBinary.size() * sizeof(uint32_t),
                shaderStageFile.spirvBinary.data(), &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't create shader reflection module");

            uint32_t pushConstantCount = 0;
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve push constants");

            std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCount);
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve push constants");

            for (auto pushConstant : pushConstants) {
                PushConstantRange range;
                range.name.assign(pushConstant->name);
                range.range.offset = pushConstant->offset;
                range.range.size = pushConstant->size;
                range.range.stageFlags = shaderModule.shaderStageFlag;
                shaderModule.pushConstantRanges.push_back(range);
            }

            // Sort by offset to make compatibility checking easy
            std::sort(shaderModule.pushConstantRanges.begin(), shaderModule.pushConstantRanges.end(),
                [](PushConstantRange& range0, PushConstantRange& range1) {
                    return range0.name < range1.name;
                });

            spvReflectDestroyShaderModule(&module);

        }

        bool Shader::CheckPushConstantsStageCompatibility() {

            /*
            for(size_t i = 1; i < shaderModules.size(); i++) {
                auto& ranges = shaderModules[i].pushConstantRanges;
                auto& prevRanges = shaderModules[i - 1].pushConstantRanges;

                if (ranges.size() != prevRanges.size())
            }
            */

            return true;

        }

    }

}