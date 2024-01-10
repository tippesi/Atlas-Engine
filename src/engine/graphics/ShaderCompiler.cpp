#include "ShaderCompiler.h"
#include "GraphicsDevice.h"
#include "Log.h"
#include "loader/AssetLoader.h"

#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv-tools/optimizer.hpp>

namespace Atlas {

    namespace Graphics {

        void InitBuildInResources(TBuiltInResource &Resources);
        EShLanguage FindLanguage(const VkShaderStageFlagBits shaderType);
        void LogError(const ShaderStageFile &shaderStageFile, const std::vector<std::string>& macros,
            glslang::TShader& shader);

        std::unordered_map<size_t, ShaderCompiler::SpvCacheEntry> ShaderCompiler::cache;
        const std::string ShaderCompiler::cachePath = ".cache/";

        void ShaderCompiler::Init() {

            glslang::InitializeProcess();

        }

        void ShaderCompiler::Shutdown() {

            glslang::FinalizeProcess();

            for (auto& [_, entry] : cache) {
                if (!entry.wasModified) continue;

                auto fileStream = Loader::AssetLoader::WriteFile(entry.fileName, std::ios::out | std::ios::binary);
                if (fileStream.is_open()) {
                    fileStream.write(reinterpret_cast<char*>(entry.spirvBinary.data()),
                        entry.spirvBinary.size() * sizeof(uint32_t));

                    fileStream.close();
                }
            }

        }

        std::vector<uint32_t> ShaderCompiler::Compile(const ShaderStageFile& shaderFile,
            const std::vector<std::string>& macros, bool useCache, bool& success) {

            if (useCache) {
                auto cacheHit = TryFindCacheEntry(shaderFile, macros, success);
                if (success) {
                    return cacheHit.spirvBinary;
                }
            }

            auto device = GraphicsDevice::DefaultDevice;

            std::vector<uint32_t> spirvBinary;

            TBuiltInResource Resources = {};
            InitBuildInResources(Resources);

            EShLanguage stage = FindLanguage(shaderFile.shaderStage);
            glslang::TShader shader(stage);
            
            if (device->support.hardwareRayTracing) {
                // Mac struggles with Spv 1.4, so use only when necessary
                shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
            }
            else {
                shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
            }

            // Enable SPIR-V and Vulkan rules when parsing GLSL
            EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

            auto glslCode = shaderFile.GetGlslCode(macros);
            const char* shaderStrings[] = { glslCode.data() };
            shader.setStrings(shaderStrings, 1);
            shader.getIntermediate()->addSourceText(glslCode.data(), glslCode.size());

            if (!shader.parse(&Resources, 100, false, messages)) {
                LogError(shaderFile, macros, shader);
                success = false;
                return spirvBinary;
            }

            glslang::TProgram program;
            program.addShader(&shader);

            if (!program.link(messages)) {
                LogError(shaderFile, macros, shader);
                success = false;
                return spirvBinary;
            }

            glslang::SpvOptions options;
            glslang::GlslangToSpv(*program.getIntermediate(stage), spirvBinary, &options);

            success = true;

            spvtools::Optimizer opt(SPV_ENV_VULKAN_1_2);
            opt.RegisterPerformancePasses();

            std::vector<uint32_t> optimizedBinary;
            if (opt.Run(spirvBinary.data(), spirvBinary.size(), &optimizedBinary)) {
                AddCacheEntry(shaderFile, macros, optimizedBinary);
                return optimizedBinary;
            }

            AddCacheEntry(shaderFile, macros, optimizedBinary);
            return spirvBinary;

        }

        ShaderCompiler::SpvCacheEntry ShaderCompiler::TryFindCacheEntry(
            const ShaderStageFile& shaderFile, const std::vector<std::string>& macros, bool& success) {

            auto hash = CalculateHash(shaderFile, macros);

            success = true;

            SpvCacheEntry entry;
            if (cache.contains(hash)) {
                entry = cache[hash];                
            }
            else {
                auto path = cachePath + std::to_string(hash);
                if (Loader::AssetLoader::FileExists(path)) {
                    entry.fileName = path;
                    entry.lastModified = Loader::AssetLoader::GetFileLastModifiedTime(path, std::filesystem::file_time_type::min());

                    auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);
                    if (fileStream.is_open()) {
                        auto content = Loader::AssetLoader::GetFileContent(fileStream);
                        fileStream.close();

                        // Quick and dirty memcpy
                        entry.spirvBinary.resize(content.size() / sizeof(uint32_t));
                        std::memcpy(entry.spirvBinary.data(), content.data(), content.size());
                    }
                    else {
                        success = false;
                    }
                }
                else {
                    success = false;
                }
            }

            success &= shaderFile.lastModified <= entry.lastModified;
            return entry;

        }

        void ShaderCompiler::AddCacheEntry(const ShaderStageFile& shaderFile,
            const std::vector<std::string>& macros, const std::vector<uint32_t>& binary) {

            auto hash = CalculateHash(shaderFile, macros);

            SpvCacheEntry entry {
                .fileName = cachePath + std::to_string(hash),

                .lastModified = shaderFile.lastModified,
                .spirvBinary = binary,

                .wasModified = true
            };
            cache[hash] = entry;

        }

        Hash ShaderCompiler::CalculateHash(const ShaderStageFile& shaderFile,
            const std::vector<std::string>& macros) {

            auto envMacros = shaderFile.GetEnvironmentMacros();

            Hash hash = 0;
            HashCombine(hash, shaderFile.filename);
            for (auto& macro : macros)
                HashCombine(hash, macro);
            for (auto& macro : envMacros)
                HashCombine(hash, macro);

            return hash;

        }

        void InitBuildInResources(TBuiltInResource &Resources) {
            Resources.maxLights = 32;
            Resources.maxClipPlanes = 6;
            Resources.maxTextureUnits = 32;
            Resources.maxTextureCoords = 32;
            Resources.maxVertexAttribs = 64;
            Resources.maxVertexUniformComponents = 4096;
            Resources.maxVaryingFloats = 64;
            Resources.maxVertexTextureImageUnits = 32;
            Resources.maxCombinedTextureImageUnits = 80;
            Resources.maxTextureImageUnits = 32;
            Resources.maxFragmentUniformComponents = 4096;
            Resources.maxDrawBuffers = 32;
            Resources.maxVertexUniformVectors = 128;
            Resources.maxVaryingVectors = 8;
            Resources.maxFragmentUniformVectors = 16;
            Resources.maxVertexOutputVectors = 16;
            Resources.maxFragmentInputVectors = 15;
            Resources.minProgramTexelOffset = -8;
            Resources.maxProgramTexelOffset = 7;
            Resources.maxClipDistances = 8;
            Resources.maxComputeWorkGroupCountX = 65535;
            Resources.maxComputeWorkGroupCountY = 65535;
            Resources.maxComputeWorkGroupCountZ = 65535;
            Resources.maxComputeWorkGroupSizeX = 1024;
            Resources.maxComputeWorkGroupSizeY = 1024;
            Resources.maxComputeWorkGroupSizeZ = 64;
            Resources.maxComputeUniformComponents = 1024;
            Resources.maxComputeTextureImageUnits = 16;
            Resources.maxComputeImageUniforms = 8;
            Resources.maxComputeAtomicCounters = 8;
            Resources.maxComputeAtomicCounterBuffers = 1;
            Resources.maxVaryingComponents = 60;
            Resources.maxVertexOutputComponents = 64;
            Resources.maxGeometryInputComponents = 64;
            Resources.maxGeometryOutputComponents = 128;
            Resources.maxFragmentInputComponents = 128;
            Resources.maxImageUnits = 8;
            Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
            Resources.maxCombinedShaderOutputResources = 8;
            Resources.maxImageSamples = 0;
            Resources.maxVertexImageUniforms = 0;
            Resources.maxTessControlImageUniforms = 0;
            Resources.maxTessEvaluationImageUniforms = 0;
            Resources.maxGeometryImageUniforms = 0;
            Resources.maxFragmentImageUniforms = 8;
            Resources.maxCombinedImageUniforms = 8;
            Resources.maxGeometryTextureImageUnits = 16;
            Resources.maxGeometryOutputVertices = 256;
            Resources.maxGeometryTotalOutputComponents = 1024;
            Resources.maxGeometryUniformComponents = 1024;
            Resources.maxGeometryVaryingComponents = 64;
            Resources.maxTessControlInputComponents = 128;
            Resources.maxTessControlOutputComponents = 128;
            Resources.maxTessControlTextureImageUnits = 16;
            Resources.maxTessControlUniformComponents = 1024;
            Resources.maxTessControlTotalOutputComponents = 4096;
            Resources.maxTessEvaluationInputComponents = 128;
            Resources.maxTessEvaluationOutputComponents = 128;
            Resources.maxTessEvaluationTextureImageUnits = 16;
            Resources.maxTessEvaluationUniformComponents = 1024;
            Resources.maxTessPatchComponents = 120;
            Resources.maxPatchVertices = 32;
            Resources.maxTessGenLevel = 64;
            Resources.maxViewports = 16;
            Resources.maxVertexAtomicCounters = 0;
            Resources.maxTessControlAtomicCounters = 0;
            Resources.maxTessEvaluationAtomicCounters = 0;
            Resources.maxGeometryAtomicCounters = 0;
            Resources.maxFragmentAtomicCounters = 8;
            Resources.maxCombinedAtomicCounters = 8;
            Resources.maxAtomicCounterBindings = 1;
            Resources.maxVertexAtomicCounterBuffers = 0;
            Resources.maxTessControlAtomicCounterBuffers = 0;
            Resources.maxTessEvaluationAtomicCounterBuffers = 0;
            Resources.maxGeometryAtomicCounterBuffers = 0;
            Resources.maxFragmentAtomicCounterBuffers = 1;
            Resources.maxCombinedAtomicCounterBuffers = 1;
            Resources.maxAtomicCounterBufferSize = 16384;
            Resources.maxTransformFeedbackBuffers = 4;
            Resources.maxTransformFeedbackInterleavedComponents = 64;
            Resources.maxCullDistances = 8;
            Resources.maxCombinedClipAndCullDistances = 8;
            Resources.maxSamples = 4;
            Resources.maxMeshOutputVerticesNV = 256;
            Resources.maxMeshOutputPrimitivesNV = 512;
            Resources.maxMeshWorkGroupSizeX_NV = 32;
            Resources.maxMeshWorkGroupSizeY_NV = 1;
            Resources.maxMeshWorkGroupSizeZ_NV = 1;
            Resources.maxTaskWorkGroupSizeX_NV = 32;
            Resources.maxTaskWorkGroupSizeY_NV = 1;
            Resources.maxTaskWorkGroupSizeZ_NV = 1;
            Resources.maxMeshViewCountNV = 4;
            Resources.limits.nonInductiveForLoops = true;
            Resources.limits.whileLoops = true;
            Resources.limits.doWhileLoops = true;
            Resources.limits.generalUniformIndexing = true;
            Resources.limits.generalAttributeMatrixVectorIndexing = true;
            Resources.limits.generalVaryingIndexing = true;
            Resources.limits.generalSamplerIndexing = true;
            Resources.limits.generalVariableIndexing = true;
            Resources.limits.generalConstantMatrixVectorIndexing = true;
        }

        EShLanguage FindLanguage(const VkShaderStageFlagBits shaderType) {
            switch (shaderType) {
                case VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT:
                    return EShLangVertex;
                case VkShaderStageFlagBits::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                    return EShLangTessControl;
                case VkShaderStageFlagBits::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                    return EShLangTessEvaluation;
                case VkShaderStageFlagBits::VK_SHADER_STAGE_GEOMETRY_BIT:
                    return EShLangGeometry;
                case VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT:
                    return EShLangFragment;
                case VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT:
                    return EShLangCompute;
                default:
                    return EShLangCompute;
            }
        }

        void LogError(const ShaderStageFile &shaderStageFile, const std::vector<std::string>& macros,
            glslang::TShader& shader) {

            std::string log;
            if (shaderStageFile.shaderStage == VK_SHADER_STAGE_VERTEX_BIT) {
                log.append("Compiling vertex stage failed.");
            }
            else if (shaderStageFile.shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
                log.append("Compiling fragment stage failed.");
            }
            else if (shaderStageFile.shaderStage == VK_SHADER_STAGE_GEOMETRY_BIT) {
                log.append("Compiling geometry stage failed.");
            }
            else if (shaderStageFile.shaderStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
                log.append("Compiling tessellation control stage failed.");
            }
            else if (shaderStageFile.shaderStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
                log.append("Compiling tessellation evaluation stage failed.");
            }
            else if (shaderStageFile.shaderStage == VK_SHADER_STAGE_COMPUTE_BIT) {
                log.append("Compiling compute stage failed.");
            }

            int32_t lineCount = 1;
            size_t pos = 0, lastPos = 0;
            log.append("\nFile: " + shaderStageFile.filename);

            auto stageCode = shaderStageFile.GetGlslCode(macros);
            while ((pos = stageCode.find('\n', lastPos)) != std::string::npos) {
                log.append("[" + std::to_string(lineCount++) + "] ");
                log.append(stageCode.substr(lastPos, pos - lastPos + 1));
                lastPos = pos + 1;
            }

            Log::Error(log);

            std::string infoLog(shader.getInfoLog());
            if (!infoLog.empty()) {
                Log::Error(infoLog);
            }
            std::string debugInfoLog(shader.getInfoDebugLog());
            if (!debugInfoLog.empty()) {
                Log::Error(debugInfoLog);
            }
            if (infoLog.empty() && debugInfoLog.empty()) {
                Log::Error("Shader compilation failed with unknown error");
            }

        }

    }

}