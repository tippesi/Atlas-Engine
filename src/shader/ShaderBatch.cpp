#include "ShaderBatch.h"
#include <algorithm>

ShaderBatch::ShaderBatch() {

	boundShaderID = 0;

}

void ShaderBatch::AddComponent(int32_t type, string filename) {

	ShaderSource* source = new ShaderSource(type, filename);

	components.push_back(source);

}

void ShaderBatch::AddConfig(ShaderConfig* config) {

	if (config->added) {
		return;
	}

	std::sort(config->macros.begin(), config->macros.end());

	for (ShaderConfigBatch* batch : configBatches) {
		if (batch->shader->macros.size() != config->macros.size()) {
			continue;
		}
		int32_t i;
		for (i = 0; i < config->macros.size(); i++) {
			if (config->macros.at(i) != batch->shader->macros.at(i)) {
				i = -1;
				break;
			}
		}
		if (i >= 0) {
			// We found an existing shader
			batch->Add(config);
			config->added = true;
			return;
		}
	}

	// No batch was found, we need to create a new shader
	Shader* shader = new Shader();
	ShaderConfigBatch* batch = new ShaderConfigBatch(shader);

	for (ShaderSource* component : components) {
		ShaderSource* componentCopy = new ShaderSource(component);
		shader->AddComponent(componentCopy);
	}

	// Now we set the macros
	for (string& macro : config->macros) {
		shader->AddMacro(macro);
	}

	shader->Compile();

	// Find the uniforms and generalize it
	for (Uniform* uniform : uniforms) {
		shader->GetUniform(uniform->name);
	}

	batch->ID = (int32_t)configBatches.size();

	batch->Add(config);
	config->added = true;
	configBatches.push_back(batch);

}

void ShaderBatch::RemoveConfig(ShaderConfig* config) {

	if (!config->added) {
		return;
	}

	ShaderConfigBatch* batch = configBatches[config->batchID];
	batch->Remove(config);

	config->added = false;

	// Remove empty batches
	if (batch->configs.size() == 0) {
		configBatches.erase(configBatches.begin() + batch->ID);
	}

}

Uniform* ShaderBatch::GetUniform(string uniformName) {

	for (ShaderConfigBatch* batch : configBatches) {
		batch->shader->GetUniform(uniformName);
	}

	// We don't care about the shader ID because the uniform object
	// is just a layer of abstraction for the renderer unlike the create
	// uniforms in the loop above.
	Uniform* uniform = new Uniform(0, uniformName, this, (int32_t)uniforms.size());
	uniforms.push_back(uniform);

	return uniform;

}

void ShaderBatch::Bind(int32_t shaderID) {

	boundShaderID = shaderID;
	configBatches.at(shaderID)->shader->Bind();

}