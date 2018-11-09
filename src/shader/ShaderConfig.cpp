#include "ShaderConfig.h"

ShaderConfig::ShaderConfig() {



}

void ShaderConfig::AddMacro(const char* macro) {

	macros.push_back(string(macro));

}

void ShaderConfig::RemoveMacro(const char* macro) {

	string macroString(macro);

	for (auto iterator = macros.begin(); iterator != macros.end(); iterator++) {
		if (macroString == *iterator) {
			macros.erase(iterator);
			return;
		}
	}

}