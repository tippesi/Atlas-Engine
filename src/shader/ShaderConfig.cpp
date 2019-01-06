#include "ShaderConfig.h"

ShaderConfig::ShaderConfig() {

	configBatchID = 0;
	added = false;

}

void ShaderConfig::AddMacro(string macro) {

	macros.push_back(macro);

}

void ShaderConfig::RemoveMacro(string macro) {

	for (auto iterator = macros.begin(); iterator != macros.end(); iterator++) {
		if (macro == *iterator) {
			macros.erase(iterator);
			return;
		}
	}

}

bool ShaderConfig::HasMacro(string macro) {

	for (string compMacro : macros) {
		if (compMacro == macro) {
			return true;
		}
	}

	return false;

}

void ShaderConfig::ClearMacros() {

	macros.clear();

}