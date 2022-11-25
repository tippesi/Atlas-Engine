#include "ShaderConfigBatch.h"

namespace Atlas {

	namespace Shader {

		ShaderConfigBatch::ShaderConfigBatch(Shader* shader) : shader(shader) {

			ID = 0;

		}

		ShaderConfigBatch::~ShaderConfigBatch() {

			delete shader;

		}

		void ShaderConfigBatch::Add(ShaderConfig* config) {

			config->shaderID = ID;
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

		size_t ShaderConfigBatch::GetSize() {

			return configs.size();

		}

		void ShaderConfigBatch::Bind() {

			shader->Bind();

		}

		Shader* ShaderConfigBatch::GetShader() {

			return shader;

		}

	}

}