#ifndef SHADERCONFIG_H
#define SHADERCONFIG_H

#include "../System.h"
#include <vector>
#include <string>

class ShaderConfig {

public:
	ShaderConfig();

	void AddMacro(string macro);

	void RemoveMacro(string macro);

	bool HasMacro(string macro);

	void ClearMacros();

	int32_t batchID;
	vector<string> macros;
	
	bool added;

};

#endif