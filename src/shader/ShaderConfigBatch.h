#ifndef SHADERCONFIGBATCH_H
#define SHADERCONFIGBATCH_H

#include "../System.h"
#include "ShaderConfig.h"
#include "Shader.h"

#include <vector>

class ShaderConfigBatch {

public:
	ShaderConfigBatch(Shader* shader);

	void Add(ShaderConfig* config);

	void Remove(ShaderConfig* config);

	int32_t ID;

	Shader* shader;
	vector<ShaderConfig*> configs;
	vector<Uniform*> uniforms;

};


#endif