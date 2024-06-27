#pragma once

#include "Common.h"
#include "Shader.h"
#include "common/Hash.h"

#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <ctime>

namespace Atlas {

    namespace Graphics {

        class ShaderCompiler {

        public:
            static void Init();

            static void Shutdown();

            static std::vector<uint32_t> Compile(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, bool useCache, bool& success);

            static bool includeDebugInfo;

        private:
            struct SpvCacheEntry {
                std::string fileName;

                std::time_t lastModified;
                std::vector<uint32_t> spirvBinary;

                bool wasModified = false;
            };

            static SpvCacheEntry TryFindCacheEntry(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, bool& success);

            static void AddCacheEntry(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, const std::vector<uint32_t>& binary);

            static Hash CalculateHash(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros);

            static std::unordered_map<size_t, SpvCacheEntry> cache;

            static const std::string cachePath;

        };

    }

}