#include "ShaderBatch.h"
#include <algorithm>

ShaderBatch::ShaderBatch() {



}

void ShaderBatch::AddComponent(int32_t type, const char* filename) {

	ShaderSource* source = new ShaderSource(type, filename);

	components.push_back(source);

}

void ShaderBatch::AddConfig(ShaderConfig* config) {

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
	for (string macro : config->macros) {
		shader->AddMacro(macro.c_str());
	}

	shader->Compile();

	// Find the uniforms and generalize it
	for (Uniform* uniform : uniforms) {
		Uniform* batchUniform = shader->GetUniform(uniform->name);
		batch->uniforms.push_back(batchUniform);
	}

	batch->Add(config);
	configBatches.push_back(batch);

}

void ShaderBatch::RemoveConfig(ShaderConfig* config) {



}

Uniform* ShaderBatch::GetUniform(const char* uniformName) {

	for (ShaderConfigBatch* batch : configBatches) {
		Uniform* uniform = batch->shader->GetUniform(uniformName);
		batch->uniforms.push_back(uniform);
	}

	// We don't care about the shader ID because the uniform object
	// is just a layer of abstraction for the renderer
	Uniform* uniform = new Uniform(0, uniformName, this, uniforms.size());
	uniforms.push_back(uniform);

	return uniform;

}