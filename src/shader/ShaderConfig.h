#ifndef SHADERCONFIG_H
#define SHADERCONFIG_H

#include "../System.h"
#include <vector>

class ShaderConfig {

public:
	ShaderConfig();

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	bool HasMacro(const char* macro);

	int32_t batchID;
	vector<string> macros;
	
	bool added;

};

#endif