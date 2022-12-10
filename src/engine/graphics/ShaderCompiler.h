#ifndef AE_GRAPHICSSHADERCOMPILER_H
#define AE_GRAPHICSSHADERCOMPILER_H

#include "Common.h"
#include "Shader.h"

#include <glslang/SPIRV/GlslangToSpv.h>

namespace Atlas {

    namespace Graphics {

        class ShaderCompiler {

        public:
            static void Init();

            static void Shutdown();

            static void Compile(ShaderStageFile& shaderFile);

        private:
            static void InitBuildInResources(TBuiltInResource &Resources);

            static EShLanguage FindLanguage(const VkShaderStageFlagBits shaderType);

        };

    }

}

#endif
