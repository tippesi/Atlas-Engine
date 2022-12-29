#include "ShaderBatch.h"
#include <algorithm>

namespace Atlas {

	namespace OldShader {

		ShaderBatch::ShaderBatch() {

			boundShaderID = 0;

		}

		ShaderBatch::~ShaderBatch() {

			for (auto& uniform : uniforms) {
				delete uniform;
			}

		}

		void ShaderBatch::AddStage(int32_t type, const std::string& filename){

			auto source = new ShaderStage(type, filename);

			stages.push_back(source);

		}

		void ShaderBatch::AddConfig(ShaderConfig* config) {

			if (config->added) {
				return;
			}

			for (auto& [key, configBatch] : configBatches) {
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
			auto shader = new OldShader();
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

			// This should never reach 2+ billion
			batch->ID = (int32_t)totalBatchCounter++;

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
				delete batch;
				configBatches.erase(config->shaderID);
			}

		}

		OldShader* ShaderBatch::GetShader(int32_t shaderID) {

			return configBatches[shaderID]->GetShader();

		}

		Uniform* ShaderBatch::GetUniform(const std::string& name) {

			// Check if equivalent uniform exists
			auto it = std::find_if(uniforms.begin(), uniforms.end(),
				[name](const auto& uniform) { return uniform->name == name; });

			if (it != uniforms.end())
				return *it;

			for (auto batchKey : configBatches) {
				batchKey.second->GetShader()->GetUniform(name);
			}

			// We don't care about the shader ID because the uniform object
			// is just a layer of abstraction for the renderer unlike the create
			// uniforms in the loop above.
			auto uniform = new Uniform(0, name, this, (int32_t)uniforms.size());
			uniforms.push_back(uniform);

			return uniform;

		}

		void ShaderBatch::Bind(ShaderConfig* config) {

			Bind(config->shaderID);

		}

		void ShaderBatch::Bind(int32_t shaderID) {

			if (configBatches.find(shaderID) != configBatches.end()) {

				boundShaderID = shaderID;

				configBatches[shaderID]->Bind();

			}

		}

	}

}