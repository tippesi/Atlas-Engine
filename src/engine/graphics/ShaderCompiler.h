#pragma once

#include "Common.h"
#include "Shader.h"
#include "common/Hash.h"

#include <stdint.h>
#include <vector>
#include <unordered_map>

namespace Atlas {

    namespace Graphics {

        class ShaderCompiler {

        public:
            static void Init();

            static void Shutdown();

            static std::vector<uint32_t> Compile(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, bool useCache, bool& success);

        private:
            struct SpvCacheEntry {
                std::string fileName;

                std::filesystem::file_time_type lastModified;
                std::vector<uint32_t> spirvBinary;

                bool wasModified = false;
            };

            static SpvCacheEntry TryFindCacheEntry(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, bool& success);

            static void AddCacheEntry(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros, std::vector<uint32_t> binary);

            static Hash CalculateHash(const ShaderStageFile& shaderFile,
                const std::vector<std::string>& macros);

            static std::unordered_map<size_t, SpvCacheEntry> cache;

            static const std::string cachePath;

        };

    }

}