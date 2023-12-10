#include "Shader.h"
#include "Extensions.h"
#include "ShaderCompiler.h"
#include "GraphicsDevice.h"

#include "../common/Hash.h"
#include "../common/Path.h"
#include "../loader/ShaderLoader.h"
#include "../loader/AssetLoader.h"

#include <spirv_reflect.h>
#include <cassert>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace Atlas {

    namespace Graphics {

        const std::string ShaderStageFile::GetGlslCode(const std::vector<std::string>& macros) const {

            auto device = GraphicsDevice::DefaultDevice;

            std::string glslCode = "";
            glslCode.append("#version 460\n\n");

            if (Extensions::IsSupported("GL_EXT_texture_shadow_lod")) {
                glslCode.append("#define AE_TEXTURE_SHADOW_LOD\n");
            }

            if (device->support.shaderPrintf) {
                glslCode.append("#define AE_SHADER_PRINTF\n");
            }

            if (device->support.hardwareRayTracing) {
                glslCode.append("#define AE_HARDWARE_RAYTRACING\n");
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

        Shader::Shader(GraphicsDevice *device, const ShaderDesc &desc) :
            device(device), shaderStageFiles(desc.stages) {



        }

        Shader::~Shader() {



        }

        Ref<ShaderVariant> Shader::GetVariant() {

            std::vector<std::string> macros;
            return GetVariant(macros);

        }

        Ref<ShaderVariant> Shader::GetVariant(std::vector<std::string> macros) {

            std::sort(macros.begin(), macros.end());

            {
                std::lock_guard lock(variantMutex);

                auto existingVariant = FindVariant(macros);

                if (existingVariant) return existingVariant;
            }

            auto variant = std::make_shared<ShaderVariant>(device, shaderStageFiles, macros);

            // Rollback in case there is a history and shader variant didn't compile
            if (!variant->isComplete && historyShaderStageFiles.size()) {
                Log::Warning("Rolling back shader due to compilation failure");

                {
                    std::lock_guard lock(variantMutex);
                    shaderStageFiles = historyShaderStageFiles;
                    historyShaderStageFiles.clear();
                    shaderVariants.clear();
                }

                variant = std::make_shared<ShaderVariant>(device, shaderStageFiles, macros);
            }

            // In the meanwhile another process could have created this variant, check again
            {
                std::lock_guard lock(variantMutex);

                if (!FindVariant(macros)) {
                    shaderVariants.push_back(variant);
                }
            }

            return variant;

        }

        bool Shader::Reload(std::unordered_map<std::string, std::filesystem::file_time_type>& lastModifiedMap) {

            std::lock_guard lock(variantMutex);

            std::filesystem::file_time_type maxLastModified = lastReload;

            bool reload = false;
            for (auto& shaderStage : shaderStageFiles) {
                std::filesystem::file_time_type lastModified;
                if (Loader::ShaderLoader::CheckForReload(shaderStage.filename,
                    shaderStage.lastModified, lastModified)) {
                    maxLastModified = std::max(maxLastModified, lastModified);
                    reload = true;
                }

                for (auto& includePath : shaderStage.includes) {
                    auto lastModifiedIt = lastModifiedMap.find(includePath);

                    if (lastModifiedIt == lastModifiedMap.end()) continue;

                    if (lastModifiedIt->second > shaderStage.lastModified)
                        reload = true;

                    maxLastModified = std::max(maxLastModified, lastModifiedIt->second);
                }
            }

            // This avoids consecutive reloads after a rollback
            if (maxLastModified <= lastReload)
                reload = false;
            else
                lastReload = maxLastModified;

            std::vector<ShaderStageFile> newShaderStageFiles;
            if (reload) {
                for (auto& shaderStage : shaderStageFiles) {
                    newShaderStageFiles.push_back(
                        Loader::ShaderLoader::LoadFile(shaderStage.filename, shaderStage.shaderStage));
                }
                // Reload means clearing all existing data
                historyShaderStageFiles = shaderStageFiles;
                shaderStageFiles = newShaderStageFiles;
                shaderVariants.clear();
            }

            return reload;

        }

        Ref<ShaderVariant> Shader::FindVariant(const std::vector<std::string> &macros) {

            for (auto& variant : shaderVariants) {
                if (variant->macros.size() != macros.size())
                    continue;

                int32_t i;
                for (i = 0; i < (int32_t)variant->macros.size(); i++) {
                    if (variant->macros.at(i) != macros.at(i)) {
                        i = -1;
                        break;
                    }
                }
                if (i >= 0) {
                    return variant;
                }
            }

            return nullptr;

        }

        ShaderVariant::ShaderVariant(GraphicsDevice* device, std::vector<ShaderStageFile>& stages,
            const std::vector<std::string>& macros) : macros(macros), device(device) {

            uint32_t allStageFlags = 0;

            std::vector<ShaderModule> shaderModules(stages.size());
            modules.resize(stages.size());
            for (size_t i = 0; i < stages.size(); i++) {
                auto& stage = stages[i];

                bool isCompiled = false;
                auto spirvBinary = ShaderCompiler::Compile(stage, macros, isCompiled);

                assert(isCompiled && "Shader compilation was unsuccessful");
                if (!isCompiled) return;

                VkShaderModuleCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.pNext = nullptr;
                createInfo.codeSize = spirvBinary.size() * sizeof(uint32_t);
                createInfo.pCode = spirvBinary.data();

                bool success = vkCreateShaderModule(device->device, &createInfo,
                    nullptr, &modules[i]) == VK_SUCCESS;
                assert(success && "Error creating shader module");
                if (!success) {
                    auto stream = Loader::AssetLoader::WriteFile("dump.txt", std::ios::out | std::ios::binary);
                    stream.write(reinterpret_cast<char*>(spirvBinary.data()), spirvBinary.size() * sizeof(uint32_t));
                    stream.close();
                    return;
                }

                // Compute shaders need a different pipeline creation
                if (stage.shaderStage != VK_SHADER_STAGE_COMPUTE_BIT)
                    isCompute = false;

                stageCreateInfos.push_back(Initializers::InitPipelineShaderStageCreateInfo(
                    stage.shaderStage, modules[i]
                ));

                shaderModules[i].shaderStageFlag = stage.shaderStage;
                GenerateReflectionData(shaderModules[i], spirvBinary);

                if (stage.shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
                    vertexInputs = shaderModules[i].inputs;

                allStageFlags |= shaderModules[i].shaderStageFlag;

            }

            std::unordered_map<std::string, PushConstantRange> ranges;
            std::unordered_map<size_t, ShaderDescriptorBinding> bindings;

            for (auto& shaderModule : shaderModules) {
                for (auto& constantRange : shaderModule.pushConstantRanges) {
                    if (ranges.contains(constantRange.name)) {
                        ranges[constantRange.name].range.stageFlags |= shaderModule.shaderStageFlag;
                    }
                    else {
                        ranges[constantRange.name] = constantRange;
                    }
                }
                for (auto& set : shaderModule.sets) {
                    for (uint32_t i = 0; i < set.bindingCount; i++) {
                        auto& binding = set.bindings[i];
                        if (bindings.contains(binding.hash)) {
                            bindings[binding.hash].layoutBinding.stageFlags |= shaderModule.shaderStageFlag;
                        }
                        else {
                            bindings[binding.hash] = binding;
                        }
                    }
                }
            }

            for (auto& [_, range] : ranges) {
                pushConstantRanges.push_back(range);
            }

            for (auto& [key, binding] : bindings) {

                auto layoutIdx = sets[binding.set].bindingCount++;

                auto idx = binding.layoutBinding.binding;
                sets[binding.set].bindings[idx] = binding;
                sets[binding.set].layoutBindings[layoutIdx] = binding.layoutBinding;

            }

            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                VkDescriptorSetLayoutCreateInfo setInfo = {};
                setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                setInfo.pNext = nullptr;
                setInfo.bindingCount = sets[i].bindingCount;
                setInfo.flags = 0;
                setInfo.pBindings = sets[i].bindingCount ? sets[i].layoutBindings : VK_NULL_HANDLE;

                VK_CHECK(vkCreateDescriptorSetLayout(device->device, &setInfo, nullptr, &sets[i].layout))
            }

            isComplete = true;

        }

        ShaderVariant::~ShaderVariant() {

            if (!isComplete) return;

            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                vkDestroyDescriptorSetLayout(device->device, sets[i].layout, nullptr);
            }

            for (auto module : modules) {
                vkDestroyShaderModule(device->device, module, nullptr);
            }

        }

        PushConstantRange* ShaderVariant::GetPushConstantRange(const std::string &name) {

            auto it = std::find_if(pushConstantRanges.begin(), pushConstantRanges.end(),
                [name](const auto& pushConstantRange) { return pushConstantRange.name == name; });

            if (it != pushConstantRanges.end())
                return &(*it);

            return nullptr;

        }

        void ShaderVariant::GenerateReflectionData(ShaderModule &shaderModule, const std::vector<uint32_t>& spirvBinary) {

            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(spirvBinary.size() * sizeof(uint32_t),
                spirvBinary.data(), &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't create shader reflection module");

            uint32_t pushConstantCount = 0;
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve push constants");

            std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCount);
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve push constants");

            for (auto pushConstant : pushConstants) {
                PushConstantRange range;
                range.name.assign(spvReflectBlockVariableTypeName(pushConstant));
                range.range.offset = pushConstant->offset;
                range.range.size = pushConstant->size;
                range.range.stageFlags = shaderModule.shaderStageFlag;
                shaderModule.pushConstantRanges.push_back(range);
            }

            // Sort by offset to make compatibility checking easy
            std::sort(shaderModule.pushConstantRanges.begin(), shaderModule.pushConstantRanges.end(),
                [](const PushConstantRange& range0, const PushConstantRange& range1) {
                    return range0.name < range1.name;
                });

            uint32_t descSetCount = 0;
            result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, nullptr);
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

            std::vector<SpvReflectDescriptorSet*> descSets(descSetCount);
            result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, descSets.data());
            assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

            for (auto descriptorSet : descSets) {
                ShaderDescriptorSet bindGroup;

                assert(descriptorSet->binding_count <= BINDINGS_PER_DESCRIPTOR_SET && "Too many bindings for this shader");

                for(uint32_t i = 0; i < descriptorSet->binding_count; i++) {
                    auto descriptorBinding = descriptorSet->bindings[i];

                    ShaderDescriptorBinding binding;

                    binding.name.assign(descriptorBinding->name);
                    binding.set = descriptorBinding->set;
                    binding.size = descriptorBinding->block.size;
                    binding.arrayElement = 0;
                    binding.valid = true;

                    assert(binding.set < DESCRIPTOR_SET_COUNT && "Too many descriptor sets for this shader");

                    binding.layoutBinding.binding = descriptorBinding->binding;
                    binding.layoutBinding.descriptorCount = descriptorBinding->count;
                    binding.layoutBinding.descriptorType = (VkDescriptorType)descriptorBinding->descriptor_type;
                    binding.layoutBinding.stageFlags = shaderModule.shaderStageFlag;

                    binding.layoutBinding.descriptorType =
                        binding.layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ?
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : binding.layoutBinding.descriptorType;

                    HashCombine(binding.hash, binding.set);
                    HashCombine(binding.hash, binding.layoutBinding.binding);

                    bindGroup.bindings[i] = binding;
                    bindGroup.bindingCount++;
                }

                shaderModule.sets.push_back(bindGroup);
            }

            if (shaderModule.shaderStageFlag == VK_SHADER_STAGE_VERTEX_BIT) {
                uint32_t inputVariableCount = 0;
                result = spvReflectEnumerateInputVariables(&module, &inputVariableCount, nullptr);
                assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

                std::vector<SpvReflectInterfaceVariable *> inputVariables(inputVariableCount);
                result = spvReflectEnumerateInputVariables(&module, &inputVariableCount, inputVariables.data());
                assert(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

                for (auto inputVariable: inputVariables) {
                    // Reject all build in variables
                    if (inputVariable->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
                        continue;
                    }

                    ShaderVertexInput input;

                    input.name.assign(inputVariable->name);
                    input.location = inputVariable->location;
                    input.format = (VkFormat)inputVariable->format;

                    shaderModule.inputs.push_back(input);
                }
            }

            spvReflectDestroyShaderModule(&module);

        }

    }

}