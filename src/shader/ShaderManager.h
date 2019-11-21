#ifndef AE_SHADERMANAGER_H
#define AE_SHADERMANAGER_H

#include "../System.h"
#include "Shader.h"

#include <mutex>
#include <map>

namespace Atlas {

	namespace Shader {

		class ShaderManager {

		public:
			static Shader* GetShader(std::string filename);

			static void ReturnShader(Shader* shader);

			static void Clear();

		private:
			struct LockableShader {

				Shader* shader;
				ShaderStage* stage;

				bool lock = false;

			};
			
			static std::map<std::string, LockableShader*> lockableShaders;
			static std::mutex mutex;

		};

	}

}

#endif