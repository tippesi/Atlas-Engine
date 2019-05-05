#include "ShaderConfig.h"

namespace Atlas {

	namespace Shader {

		ShaderConfig::ShaderConfig() {

			shaderID = 0;
			added = false;

		}

		void ShaderConfig::AddMacro(std::string macro) {

			macros.push_back(macro);

		}

		void ShaderConfig::RemoveMacro(std::string macro) {

			for (auto iterator = macros.begin(); iterator != macros.end(); iterator++) {
				if (macro == *iterator) {
					macros.erase(iterator);
					return;
				}
			}

		}

		bool ShaderConfig::HasMacro(std::string macro) {

			for (auto& compMacro : macros) {
				if (compMacro == macro) {
					return true;
				}
			}

			return false;

		}

		void ShaderConfig::ClearMacros() {

			macros.clear();

		}

	}

}