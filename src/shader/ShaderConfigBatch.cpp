#include "ShaderConfigBatch.h"

ShaderConfigBatch::ShaderConfigBatch(Shader* shader) : shader(shader) {

	ID = 0;

}

void ShaderConfigBatch::Add(ShaderConfig* config) {

	config->batchID = ID;
	configs.push_back(config);

}

void ShaderConfigBatch::Remove(ShaderConfig* config) {

	for (auto iterator = configs.begin(); iterator != configs.end(); iterator++) {
		if (config == *iterator) {
			configs.erase(iterator);
			return;
		}
	}

}