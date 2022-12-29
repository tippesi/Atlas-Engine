#include "ShaderConfig.h"

#include <algorithm>

namespace Atlas {

	namespace OldShader {

		void ShaderConfig::AddMacro(const std::string& macro) {

			macros.push_back(macro);

			std::sort(macros.begin(), macros.end());

		}

		void ShaderConfig::RemoveMacro(const std::string& macro) {

			auto it = std::find(macros.begin(), macros.end(), macro);

			if (it != macros.end())
				macros.erase(it);

		}

		bool ShaderConfig::HasMacro(const std::string& macro) {

			return std::any_of(macros.begin(), macros.end(),
				[macro](const auto& value) { return value == macro; });

		}

		void ShaderConfig::ClearMacros() {

			macros.clear();

		}

	}

}