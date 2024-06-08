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

            std::string glslCode = "";
            glslCode.append("#version 460\n\n");

            auto envMacros = GetEnvironmentMacros();

            for (const auto& macro : envMacros) {
                glslCode.append("#define " + macro + "\n");
            }

            for (const auto& macro : macros) {
                glslCode.append("#define " + macro + "\n");
            }

            for (const auto& extension : extensions) {
                for (auto& ifdef : extension.ifdefs)
                    glslCode += ifdef + "\n";
                glslCode += extension.extension + "\n";
                for (size_t i = 0; i < extension.ifdefs.size(); i++)
                    glslCode += "#endif\n";
            }

            glslCode.append(code);
            glslCode.erase(std::remove(glslCode.begin(), glslCode.end(), '\r'), glslCode.end());

            return glslCode;

        }

        const std::vector<std::string> ShaderStageFile::GetEnvironmentMacros() const {

            auto device = GraphicsDevice::DefaultDevice;

            std::vector<std::string> macros;

            if (Extensions::IsSupported("GL_EXT_texture_shadow_lod")) {
                macros.push_back("AE_TEXTURE_SHADOW_LOD");
            }
            if (device->support.shaderPrintf) {
                macros.push_back("AE_SHADER_PRINTF");
            }

            if (device->support.hardwareRayTracing) {
                macros.push_back("AE_HARDWARE_RAYTRACING");
            }

            if (device->support.bindless) {
                macros.push_back("AE_BINDLESS");
            }

#ifdef AE_OS_MACOS
            macros.push_back("AE_OS_MACOS");
#endif
#ifdef AE_OS_LINUX
            macros.push_back("AE_OS_LINUX");
#endif
#ifdef AE_OS_WINDOWS
            macros.push_back("AE_OS_WINDOWS");
#endif

            return macros;

        }

        Shader::Shader(GraphicsDevice *device, const ShaderDesc &desc) :
            name(desc.name), device(device), shaderStageFiles(desc.stages) {



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
            modules.resize(stages.size(), VK_NULL_HANDLE);
            for (size_t i = 0; i < stages.size(); i++) {
                auto& stage = stages[i];

                bool isCompiled = false;
                auto spirvBinary = ShaderCompiler::Compile(stage, macros, true, isCompiled);

                AE_ASSERT(isCompiled && "Shader compilation was unsuccessful");
                if (!isCompiled) return;

                VkShaderModuleCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.pNext = nullptr;
                createInfo.codeSize = spirvBinary.size() * sizeof(uint32_t);
                createInfo.pCode = spirvBinary.data();

                auto result = vkCreateShaderModule(device->device, &createInfo,
                    nullptr, &modules[i]);
                VK_CHECK_MESSAGE(result, "Error creating shader module");
                if (result != VK_SUCCESS) {                    
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
                            bindings[binding.hash].binding.stageFlags |= shaderModule.shaderStageFlag;
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

            DescriptorSetLayoutDesc layoutDesc[DESCRIPTOR_SET_COUNT];
            for (auto& [key, binding] : bindings) {

                auto layoutIdx = sets[binding.set].bindingCount++;

                auto idx = binding.binding.bindingIdx;
                sets[binding.set].bindings[idx] = binding;

                layoutDesc[binding.set].bindings[layoutIdx] = binding.binding;
            }

            for (uint32_t i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
                layoutDesc[i].bindingCount = sets[i].bindingCount;

                sets[i].layout = device->CreateDescriptorSetLayout(layoutDesc[i]);
            }

            isComplete = true;

        }

        ShaderVariant::~ShaderVariant() {

            for (auto module : modules) {
                if (module == VK_NULL_HANDLE) continue;

                vkDestroyShaderModule(device->device, module, nullptr);
            }

        }

        PushConstantRange* ShaderVariant::GetPushConstantRange(const std::string &name) {

            auto it = std::find_if(pushConstantRanges.begin(), pushConstantRanges.end(),
                [&](const auto& pushConstantRange) { return pushConstantRange.name == name; });

            if (it != pushConstantRanges.end())
                return &(*it);

            return nullptr;

        }

        bool ShaderVariant::TryOverrideDescriptorSetLayout(Ref<DescriptorSetLayout> layout, uint32_t set) {

            AE_ASSERT(set < DESCRIPTOR_SET_COUNT && "Descriptor set index out of range");

            if (set >= DESCRIPTOR_SET_COUNT ||
                !layout->IsCompatible(sets[set].layout)) {
                return false;
            }

            sets[set].layout = layout;
            sets[set].bindingCount = uint32_t(layout->bindings.size());

            for (size_t i = 0; i < BINDINGS_PER_DESCRIPTOR_SET; i++) {
                sets[set].bindings[i].valid = false;
            }
            
            for (size_t i = 0; i < layout->layoutBindings.size(); i++) {
                const auto& binding = layout->bindings[i];

                auto idx = binding.bindingIdx;
                sets[set].bindings[idx].binding = binding;
                sets[set].bindings[idx].valid = true;
            }

            return true;

        }

        void ShaderVariant::GenerateReflectionData(ShaderModule &shaderModule, const std::vector<uint32_t>& spirvBinary) {

            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(spirvBinary.size() * sizeof(uint32_t),
                spirvBinary.data(), &module);
            AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't create shader reflection module");

            uint32_t pushConstantCount = 0;
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);
            AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve push constants");

            std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCount);
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());
            AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve push constants");

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
            AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

            std::vector<SpvReflectDescriptorSet*> descSets(descSetCount);
            result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, descSets.data());
            AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

            for (auto descriptorSet : descSets) {
                ShaderDescriptorSet bindGroup;

                AE_ASSERT(descriptorSet->binding_count <= BINDINGS_PER_DESCRIPTOR_SET && "Too many bindings for this shader");

                for(uint32_t i = 0; i < descriptorSet->binding_count; i++) {
                    auto descriptorBinding = descriptorSet->bindings[i];

                    ShaderDescriptorBinding binding;

                    binding.name.assign(descriptorBinding->name);
                    binding.set = descriptorBinding->set;
                    
                    binding.valid = true;

                    AE_ASSERT(binding.set < DESCRIPTOR_SET_COUNT && "Too many descriptor sets for this shader");

                    binding.binding.bindingIdx = descriptorBinding->binding;
                    binding.binding.descriptorCount = descriptorBinding->count;
                    binding.binding.descriptorType = (VkDescriptorType)descriptorBinding->descriptor_type;
                    binding.binding.size = descriptorBinding->block.size;
                    binding.binding.arrayElement = 0;
                    binding.binding.stageFlags = VK_SHADER_STAGE_ALL;
                    binding.binding.bindless = descriptorBinding->array.dims_count == 1 &&
                        descriptorBinding->array.dims[0] == 1 && device->support.bindless;

                    binding.binding.descriptorType =
                        binding.binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ?
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : binding.binding.descriptorType;

                    HashCombine(binding.hash, binding.set);
                    HashCombine(binding.hash, binding.binding.bindingIdx);

                    bindGroup.bindings[i] = binding;
                    bindGroup.bindingCount++;
                }

                shaderModule.sets.push_back(bindGroup);
            }

            if (shaderModule.shaderStageFlag == VK_SHADER_STAGE_VERTEX_BIT) {
                uint32_t inputVariableCount = 0;
                result = spvReflectEnumerateInputVariables(&module, &inputVariableCount, nullptr);
                AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

                std::vector<SpvReflectInterfaceVariable *> inputVariables(inputVariableCount);
                result = spvReflectEnumerateInputVariables(&module, &inputVariableCount, inputVariables.data());
                AE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS && "Couldn't retrieve descriptor sets");

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