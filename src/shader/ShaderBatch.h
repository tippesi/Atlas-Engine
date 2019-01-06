#ifndef SHADERBATCH_H
#define SHADERBATCH_H

#include "../System.h"
#include "Shader.h"
#include "ShaderConfigBatch.h"

#include <vector>

class ShaderBatch {

public:
	/**
	 * Constructs a ShaderBatch object.
	 */
	ShaderBatch();

	/**
	 * Destructs a ShaderBatch object.
	 */
	~ShaderBatch();

    /**
     * Adds a shader stage to the shader (e.g the vertex shader)
     * @param type The type of the stage. See {@link ShaderStage.h} for more.
     * @param filename The name of the GLSL file.
     * @note All components are managed by the shader. This means that they
     * get released from memory if the shader gets destructed.
     */
	void AddStage(int32_t type, string filename);

	/**
	 * Adds a shader config to the batch.
	 * @param config A valid pointer to a ShaderConfig object.
	 */
	void AddConfig(ShaderConfig* config);

	/**
	 * Removes a shader config from the batch,
	 * @param config A valid pointer to a ShaderConfig object.
	 */
	void RemoveConfig(ShaderConfig* config);

	/**
	 * Returns a Uniform object for a specific uniform of the shader.
	 * @param uniformName The name of the uniform.
	 * @return A pointer to a Uniform object if valid. Nullptr otherwise.
	 * @note All uniforms are managed by the shader batch. This means that
	 * they get released from memory if the shader batch gets destructed.
	 */
	Uniform* GetUniform(string uniformName);

	/**
	 * Binds the shader of a config batch.
	 * @param configBatchID The ID of the config batch.
	 */
	void Bind(int32_t configBatchID);

	vector<ShaderStage*> stages;
	vector<ShaderConfigBatch*> configBatches;
	vector<Uniform*> uniforms;	

	int32_t boundShaderID;

};

#endif