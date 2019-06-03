#include "ShaderBatch.h"
#include <algorithm>

namespace Atlas {

	namespace Shader {

		ShaderBatch::ShaderBatch() {

			boundShaderID = 0;

		}

		ShaderBatch::~ShaderBatch() {

			for (auto& uniform : uniforms) {
				delete uniform;
			}

		}

		void ShaderBatch::AddStage(int32_t type, std::string filename){

			auto source = new ShaderStage(type, filename);

			stages.push_back(source);

		}

		void ShaderBatch::AddConfig(ShaderConfig* config) {

			if (config->added) {
				return;
			}

			std::sort(config->macros.begin(), config->macros.end());

			for (auto configBatchKey : configBatches) {
				auto configBatch = configBatchKey.second;
				if (configBatch->GetShader()->macros.size() != config->macros.size()) {
					continue;
				}
				int32_t i;
				for (i = 0; i < (int32_t)config->macros.size(); i++) {
					if (config->macros.at(i) != configBatch->GetShader()->macros.at(i)) {
						i = -1;
						break;
					}
				}
				if (i >= 0) {
					// We found an existing shader
					configBatch->Add(config);
					config->added = true;
					return;
				}
			}

			// No batch was found, we need to create a new shader
			auto shader = new Shader();
			auto batch = new ShaderConfigBatch(shader);

			for (auto stage : stages) {
				auto componentCopy = new ShaderStage(*stage);
				shader->AddStage(componentCopy);
			}

			// Now we set the macros
			for (auto& macro : config->macros) {
				shader->AddMacro(macro);
			}

			shader->Compile();

			// Find the uniforms and generalize it
			for (auto& uniform : uniforms) {
				shader->GetUniform(uniform->name);
			}

			batch->ID = (int32_t)configBatches.size();

			batch->Add(config);
			config->added = true;
			configBatches[batch->ID] = batch;

		}

		void ShaderBatch::RemoveConfig(ShaderConfig* config) {

			if (!config->added) {
				return;
			}

			auto batch = configBatches[config->shaderID];
			batch->Remove(config);

			config->added = false;

			// Remove empty batches
			if (batch->GetSize() == 0) {
				configBatches.erase(config->shaderID);
			}

		}

		Uniform* ShaderBatch::GetUniform(std::string uniformName) {

			for (auto batchKey : configBatches) {
				batchKey.second->GetShader()->GetUniform(uniformName);
			}

			// We don't care about the shader ID because the uniform object
			// is just a layer of abstraction for the renderer unlike the create
			// uniforms in the loop above.
			auto uniform = new Uniform(0, uniformName, this, (int32_t)uniforms.size());
			uniforms.push_back(uniform);

			return uniform;

		}

		void ShaderBatch::Bind(ShaderConfig* config) {

			Bind(config->shaderID);

		}

		void ShaderBatch::Bind(int32_t shaderID) {

			boundShaderID = shaderID;

			configBatches[shaderID]->Bind();

		}

	}

}