#include "Extensions.h"
#include "Instance.h"

namespace Atlas {

    namespace Graphics {

        std::unordered_set<std::string> Extensions::supportedExtensions;

        void Extensions::Process() {

            auto graphicsInstance = Instance::defaultInstance;

            for (auto extension : graphicsInstance->extensionNames) {
                supportedExtensions.insert(std::string(extension));
            }

        }

        bool Extensions::IsSupported(std::string extension) {

            return supportedExtensions.find(extension) != supportedExtensions.end();

        }

    }

}