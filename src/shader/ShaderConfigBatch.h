#ifndef SHADERCONFIGBATCH_H
#define SHADERCONFIGBATCH_H

#include "../System.h"
#include "ShaderConfig.h"
#include "Shader.h"

#include <vector>

class ShaderConfigBatch {

public:
	///
	/// \param shader
	ShaderConfigBatch(Shader* shader);

	///
	/// \param config
	void Add(ShaderConfig* config);

	///
	/// \param config
	void Remove(ShaderConfig* config);

	int32_t ID;

	Shader* shader;
	vector<ShaderConfig*> configs;

};


#endif