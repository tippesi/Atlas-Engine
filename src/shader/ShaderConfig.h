#ifndef AE_SHADERCONFIG_H
#define AE_SHADERCONFIG_H

#include "../System.h"
#include <vector>
#include <string>

namespace Atlas {

	namespace Shader {

		/**
		 * A shader configuration consists of macros which configure a
		 * shader in a shader batch. Shader configs with the same macros
		 * use the same shader. A ShaderConfig always belongs to a ShaderConfigBatch
 		 * in one ShaderBatch and uses the shader of this batch.
		 * @note A ShaderConfig can only be used in one ShaderBatch at a time.
		 */
		class ShaderConfig {

		public:
			/**
             * Constructs a ShaderConfig object.
             */
			ShaderConfig();

			/**
             * Adds a macro to the config.
             * @param macro The macro to be added.
             */
			void AddMacro(std::string macro);

			/**
             * Removes a macro from the config.
             * @param macro The macro to be removed.
             */
			void RemoveMacro(std::string macro);

			/**
             * Checks if a macro is present in the config.
             * @param macro The macro to be checked.
             * @return True if present, false otherwise.
             */
			bool HasMacro(std::string macro);

			/**
             * Removes all macros from the config.
             */
			void ClearMacros();

			int32_t shaderID;
			std::vector<std::string> macros;

			bool added;

		};

	}

}

#endif