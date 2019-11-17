#ifndef AE_SHADERSTAGE_H
#define AE_SHADERSTAGE_H

#include "../System.h"
#include "ShaderConstant.h"

#include <list>
#include <string>
#include <set>

#define AE_VERTEX_STAGE GL_VERTEX_SHADER
#define AE_FRAGMENT_STAGE GL_FRAGMENT_SHADER
#define AE_GEOMETRY_STAGE GL_GEOMETRY_SHADER
#define AE_TESSELLATION_CONTROL_STAGE GL_TESS_CONTROL_SHADER
#define AE_TESSELLATION_EVALUATION_STAGE GL_TESS_EVALUATION_SHADER
#define AE_COMPUTE_STAGE GL_COMPUTE_SHADER

namespace Atlas {

	namespace Shader {

		/**
		 * A shader stage is a module/stage of a shader, e.g it might be a vertex shader.
		 */
		class ShaderStage {

		public:
			/**
             * Constructs a ShaderStage object.
             * @param type The type of the stage. See {@link ShaderStage.h} for more.
             * @param filename The name of the GLSL file
             */
			ShaderStage(int32_t type, std::string filename);

			/**
             * Constructs a ShaderStage object.
             * @param source A pointer to a valid ShaderStage object.
             */
			ShaderStage(ShaderStage& source);

			/**
             * Destructs a ShaderStage object.
             */
			~ShaderStage();

			/**
             * Reloads the shader stage from the disk.
             * @return True if it had to be reloaded (timestamp), false otherwise.
             */
			bool Reload();

			/**
             * Adds a macro to the shader stage.
             * @param macro The macro to be added.
             */
			void AddMacro(std::string macro);

			/**
             * Removes a macro from the shader stage.
             * @param macro The macro to be removed.
             */
			void RemoveMacro(std::string macro);

			/**
             * Returns a ShaderConstant object for a specific constant of the shader stage.
             * @param constant The name of the constant.
             * @return A pointer to a ShaderConstant object if valid. Nullptr otherwise.
             */
			ShaderConstant* GetConstant(std::string constant);

			/**
            * Compiles the shader stage.
            * @return True if successful, false otherwise.
            */
			bool Compile();

			/**
             * Sets the root shader source/binary directory for all shader files.
             * @param directory The path to the directory
             */
			static void SetSourceDirectory(std::string directory);

#ifdef AE_SHOW_LOG
			std::string GetErrorLog();
#endif

			int32_t ID;
			int32_t type;

			std::string filename;

		private:
			std::string ReadShaderFile(std::string filename, bool mainFile);

			time_t GetLastModified();

#ifdef AE_SHOW_LOG
			std::string stageCode;
#endif

			std::string code;
			std::list<std::string> macros;
			std::list<ShaderConstant*> constants;
			std::set<std::string> includes;

			time_t lastModified;

			bool error = false;

			static std::string sourceDirectory;

		};

	}

}

#endif