#include "Extensions.h"

namespace Atlas {

    std::unordered_set<std::string> Extensions::supportedExtensions;

	void Extensions::Process() {

        int32_t extensionCount = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

        for (int32_t i = 0; i < extensionCount; i++) {
            const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
            supportedExtensions.insert(std::string(extension));
        }

	}

	bool Extensions::IsSupported(std::string extension) {

		return supportedExtensions.find(extension) != supportedExtensions.end();

	}

}