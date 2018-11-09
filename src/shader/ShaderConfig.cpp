#include "ShaderConfig.h"

ShaderConfig::ShaderConfig() {

	batchID = 0;
	added = false;

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

bool ShaderConfig::HasMacro(const char* macro) {

	string macroString(macro);

	for (string macro : macros) {
		if (macro == macroString) {
			return true;
		}
	}

	return false;

}