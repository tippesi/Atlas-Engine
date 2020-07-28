#include "ShaderConfig.h"

#include <algorithm>

namespace Atlas {

	namespace Shader {

		void ShaderConfig::AddMacro(std::string macro) {

			macros.push_back(macro);

			std::sort(macros.begin(), macros.end());

		}

		void ShaderConfig::RemoveMacro(std::string macro) {

			auto it = std::find(macros.begin(), macros.end(), macro);

			if (it != macros.end())
				macros.erase(it);

		}

		bool ShaderConfig::HasMacro(std::string macro) {

			return std::any_of(macros.begin(), macros.end(),
				[macro](const auto& value) { return value == macro; });

		}

		void ShaderConfig::ClearMacros() {

			macros.clear();

		}

	}

}