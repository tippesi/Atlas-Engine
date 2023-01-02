#include "ShaderCompiler.h"
#include "Log.h"

#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv-tools/optimizer.hpp>

namespace Atlas {

    namespace Graphics {

        void InitBuildInResources(TBuiltInResource &Resources);
        EShLanguage FindLanguage(const VkShaderStageFlagBits shaderType);
        void LogError(const ShaderStageFile &shaderStageFile, const std::vector<std::string>& macros,
            glslang::TShader& shader);

        void ShaderCompiler::Init() {

            glslang::InitializeProcess();

        }

        void ShaderCompiler::Shutdown() {

            glslang::FinalizeProcess();

        }

        std::vector<uint32_t> ShaderCompiler::Compile(const ShaderStageFile& shaderFile,
            const std::vector<std::string>& macros, bool& success) {

            std::vector<uint32_t> spirvBinary;

            TBuiltInResource Resources = {};
            InitBuildInResources(Resources);

            EShLanguage stage = FindLanguage(shaderFile.shaderStage);
            glslang::TShader shader(stage);

            // Enable SPIR-V and Vulkan rules when parsing GLSL
            EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

            auto glslCode = shaderFile.GetGlslCode(macros);
            const char* shaderStrings[] = { glslCode.data() };
            shader.setStrings(shaderStrings, 1);

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

            glslang::GlslangToSpv(*program.getIntermediate(stage), spirvBinary);
            success = true;

            spvtools::Optimizer opt(SPV_ENV_VULKAN_1_2);
            opt.RegisterPerformancePasses();

            std::vector<uint32_t> optimizedBinary;
            if (opt.Run(spirvBinary.data(), spirvBinary.size(), &optimizedBinary)) {
                return optimizedBinary;
            }

            return optimizedBinary;

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
            Resources.limits.nonInductiveForLoops = 1;
            Resources.limits.whileLoops = 1;
            Resources.limits.doWhileLoops = 1;
            Resources.limits.generalUniformIndexing = 1;
            Resources.limits.generalAttributeMatrixVectorIndexing = 1;
            Resources.limits.generalVaryingIndexing = 1;
            Resources.limits.generalSamplerIndexing = 1;
            Resources.limits.generalVariableIndexing = 1;
            Resources.limits.generalConstantMatrixVectorIndexing = 1;
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

        }

    }

}