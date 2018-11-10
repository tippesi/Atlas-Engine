#ifndef SHADERCONFIG_H
#define SHADERCONFIG_H

#include "../System.h"
#include <vector>
#include <string>

class ShaderConfig {

public:
	ShaderConfig();

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	bool HasMacro(const char* macro);

	void ClearMacros();

	int32_t batchID;
	vector<string> macros;
	
	bool added;

};

#endif