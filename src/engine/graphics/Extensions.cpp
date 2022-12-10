#include "Extensions.h"

#include "EngineInstance.h"

namespace Atlas {

    namespace Graphics {

        std::unordered_set<std::string> Extensions::supportedExtensions;

        void Extensions::Process() {

            auto graphicsInstance = EngineInstance::GetGraphicsInstance();

            for (auto extension : graphicsInstance->extensionNames) {
                supportedExtensions.insert(std::string(extension));
            }

        }

        bool Extensions::IsSupported(std::string extension) {

            return supportedExtensions.find(extension) != supportedExtensions.end();

        }

    }

}