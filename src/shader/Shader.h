#ifndef AE_SHADER_H
#define AE_SHADER_H

#include "../System.h"
#include "ShaderStage.h"
#include "Uniform.h"

#include <vector>

namespace Atlas {

	namespace Shader {

        class Shader {

        public:
            /**
             * Constructs a Shader object.
             */
            Shader();

			Shader(const Shader& that) = delete;

            /**
             * Destructs a Shader object.
             */
            ~Shader();

			/**
			 * A Shader object shouldn't be copied because of the uniforms.
			 */
			Shader& operator=(const Shader& that) = delete;

            /**
             * Adds a shader stage to the shader (e.g the vertex shader)
             * @param type The type of the stage. See {@link ShaderStage.h} for more.
             * @param filename The name of the GLSL file.
             * @note All stages are managed by the shader. This means that they
             * get released from memory if the shader gets destructed.
             */
            void AddStage(int32_t type, std::string filename);

            /**
             * Adds a shader stage to the shader (e.g the vertex shader)
             * @param stage A pointer to a ShaderStage object.
             * @note All stages are managed by the shader. This means that they
             * get released from memory if the shader gets destructed.
             */
            void AddStage(ShaderStage* stage);

            /**
             * Returns a stage
             * @param type The type of the stage. See {@link ShaderStage.h} for more.
             * @return A pointer to the ShaderStage object.
             */
            ShaderStage* GetStage(int32_t type);

            /**
             * Returns a Uniform object for a specific uniform of the shader.
             * @param uniformName The name of the uniform.
             * @return A pointer to a Uniform object if valid. Nullptr otherwise.
             * @note All uniforms are managed by the shader. This means that
             * they get released from memory if the shader gets destructed.
             */
            Uniform* GetUniform(std::string uniformName);

            /**
             * Adds a macro to the shader.
             * @param macro The macro to be added.
             */
            void AddMacro(std::string macro);

            /**
             * Removes a macro from the shader.
             * @param macro The macro to be removed.
             */
            void RemoveMacro(std::string macro);

            /**
             * Checks if a macro is present in the shader.
             * @param macro The macro to be checked.
             * @return True if present, false otherwise.
             */
            bool HasMacro(std::string macro);

            /**
             * Compiles all shader stages and links them
             * @return True if successful, false otherwise.
             */
            bool Compile();

            /**
             * Requests if the shader is successfully compiled.
             * @return True if successful, false otherwise.
             */
            bool IsCompiled();

            /**
             * Binds the shader.
             */
            void Bind();

            /**
             * Unbinds any bound shader.
             */
            void Unbind();

            uint32_t GetID();

            std::vector<Uniform*> uniforms;
            std::vector<std::string> macros;

        private:
            uint32_t ID;

            std::vector<ShaderStage*> stages;

            bool isCompiled;

            static uint32_t boundShaderID;

        };

	}

}

#endif