#ifndef AE_SHADERCONFIGBATCH_H
#define AE_SHADERCONFIGBATCH_H

#include "../System.h"
#include "ShaderConfig.h"
#include "Shader.h"

#include <vector>

namespace Atlas {

	namespace Shader {

		/**
		 * A ShaderConfigBatch groups an arbitrary number of ShaderConfigs
		 * together which use the same shader. This means all the batch only
		 * contains ShaderConfigs which have the same macros.
		 */
		class ShaderConfigBatch {

		public:
			/**
             * Constructs a ShaderConfigBatch object.
             * @param shader A pointer to a Shader object the batch should use.
             * @note The shader will be managed by the shader configuration batch.
             * This means that it wiil be released from memory if the batch gets destructed.
             */
			ShaderConfigBatch(Shader* shader);

			/**
             * Destructs a ShaderConfigBatch object.
             */
			~ShaderConfigBatch();

			/**
             * Adds a shader config to the batch.
             * @param config A valid pointer to a ShaderConfig object.
             */
			void Add(ShaderConfig* config);

			/**
             * Removes a shader config from the batch,
             * @param config A valid pointer to a ShaderConfig object.
             */
			void Remove(ShaderConfig* config);

			/**
             * Returns the number of elements the batch holds.
             * @return An integer with the number of elements
             */
			size_t GetSize();

			/**
             * Binds the shader of the batch
             */
			void Bind();

			/**
             * Returns the shader of the batch
             * @return A pointer to the shader object.
             */
			Shader* GetShader();

			/**
             * The ID of the batch
             */
			int32_t ID;

		private:
			Shader* shader;
			std::vector<ShaderConfig*> configs;

		};

	}

}

#endif