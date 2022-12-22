#ifndef AE_GRAPHICSSHADERCOMPILER_H
#define AE_GRAPHICSSHADERCOMPILER_H

#include "Common.h"
#include "Shader.h"

namespace Atlas {

    namespace Graphics {

        class ShaderCompiler {

        public:
            static void Init();

            static void Shutdown();

            static void Compile(ShaderStageFile& shaderFile);

        };

    }

}

#endif
