#ifndef SHADERCONFIG_H
#define SHADERCONFIG_H

#include "../System.h"
#include <vector>

class ShaderConfig {

public:
	ShaderConfig();

	void AddMacro(const char* macro);

	void RemoveMacro(const char* macro);

	vector<string> macros;

};

#endif