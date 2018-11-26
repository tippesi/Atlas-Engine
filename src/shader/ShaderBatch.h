#ifndef SHADERBATCH_H
#define SHADERBATCH_H

#include "../System.h"
#include "Shader.h"
#include "ShaderConfigBatch.h"

#include <vector>

class ShaderBatch {

public:
	///
	ShaderBatch();

	///
	/// \param type
	/// \param filename
	void AddComponent(int32_t type, string filename);

	///
	/// \param config
	void AddConfig(ShaderConfig* config);

	///
	/// \param config
	void RemoveConfig(ShaderConfig* config);

	///
	/// \param uniformName
	/// \return
	Uniform* GetUniform(string uniformName);

	///
	/// \param shaderID
	void Bind(int32_t shaderID);

	vector<ShaderSource*> components;
	vector<ShaderConfigBatch*> configBatches;
	vector<Uniform*> uniforms;	

	int32_t boundShaderID;

};

#endif