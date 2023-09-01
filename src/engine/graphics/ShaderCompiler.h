#pragma once

#include "Common.h"
#include "Shader.h"

#include <stdint.h>
#include <vector>

namespace Atlas {

    namespace Graphics {

        class ShaderCompiler {

        public:
            static void Init();

            static void Shutdown();

            static std::vector<uint32_t> Compile(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, bool& success);

        };

    }

}