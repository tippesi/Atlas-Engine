#pragma once

#include "System.h"

#include <unordered_set>

namespace Atlas {

    namespace Graphics {

        class Extensions {

        public:
            /**
             * Processes all API extensions
             * @note This is automatically called at engine startup
             */
            static void Process();

            /**
             * Checks whether an extension is supported
             * @param extension The extension to be checked
             * @return True if the extension is supported, false otherwise
             */
            static bool IsSupported(const std::string& extension);

        private:
            static std::unordered_set<std::string> supportedExtensions;

        };

    }

}