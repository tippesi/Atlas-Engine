#ifndef SHADERCONFIG_H
#define SHADERCONFIG_H

#include "../System.h"
#include <vector>
#include <string>

class ShaderConfig {

public:
	///
	ShaderConfig();

	///
	/// \param macro
	void AddMacro(string macro);

	///
	/// \param macro
	void RemoveMacro(string macro);

	///
	/// \param macro
	/// \return
	bool HasMacro(string macro);

	///
	void ClearMacros();

	int32_t batchID;
	vector<string> macros;
	
	bool added;

};

#endif