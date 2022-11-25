#include "ShaderManager.h"

namespace Atlas {

	namespace Shader {

		std::map<std::string, ShaderManager::LockableShader*> ShaderManager::lockableShaders;
		std::mutex ShaderManager::mutex;

		Shader* ShaderManager::GetShader(std::string filename) {

			std::lock_guard<std::mutex> guard(mutex);

			auto key = lockableShaders.find(filename);

			if (key != lockableShaders.end()) {
				auto lockable = key->second;

				// Main shader is already in use, create secondary
				if (lockable->lock) {
					auto shader = new Shader();
					auto shaderStage = new ShaderStage(*lockable->stage);
					shader->AddStage(lockable->stage);
					shader->name = filename;
					return shader;
				}

				lockable->lock = true;
				return lockable->shader;
			}

			// No shader was found, we can create a new one.
			auto lockable = new LockableShader;

			lockable->shader = new Shader();
			lockable->stage = new ShaderStage(AE_COMPUTE_STAGE, filename);

			lockable->shader->name = filename;
			lockable->lock = true;

			lockableShaders[filename] = lockable;

			return lockable->shader;

		}

		void ShaderManager::ReturnShader(Shader* shader) {

			std::lock_guard<std::mutex> guard(mutex);

			auto key = lockableShaders.find(shader->name);

			// No such shader was found
			if (key == lockableShaders.end())
				return;

			auto lockable = key->second;

			if (lockable->shader == shader) {
				// Primary shader was returned
				lockable->lock = false;
			}
			else {
				// Secondary shader was returned, delete it.
				delete shader;
			}

		}

		void ShaderManager::Clear() {

			std::lock_guard<std::mutex> guard(mutex);

			for (auto& key : lockableShaders) {
				delete key.second->shader;
				delete key.second;
			}

			lockableShaders.clear();

		}

	}

}