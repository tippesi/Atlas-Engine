#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../RenderList.h"
#include "../shader/ShaderBatch.h"

#include "Renderer.h"
#include "ImpostorRenderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class OpaqueRenderer : public Renderer {

		public:
			OpaqueRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
				Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap);

			void RenderImpostor(Viewport* viewport, Framebuffer* framebuffer, std::vector<mat4> viewMatrices,
				mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

			static void InitShaderBatch();

			static void AddConfig(Shader::ShaderConfig* config);

			static void RemoveConfig(Shader::ShaderConfig* config);

		private:
			void AdjustFaceCulling(bool cullFaces, bool& state);

			RenderList renderList;

			ImpostorRenderer impostorRenderer;

			Shader::Uniform* modelMatrixUniform = nullptr;
			Shader::Uniform* viewMatrixUniform = nullptr;
			Shader::Uniform* projectionMatrixUniform = nullptr;

			Shader::Uniform* cameraLocationUniform = nullptr;
			Shader::Uniform* baseColorUniform = nullptr;
			Shader::Uniform* roughnessUniform = nullptr;
			Shader::Uniform* metalnessUniform = nullptr;
			Shader::Uniform* aoUniform = nullptr;
			Shader::Uniform* normalScaleUniform = nullptr;
			Shader::Uniform* displacementScaleUniform = nullptr;
			Shader::Uniform* materialIdxUniform = nullptr;

			Shader::Uniform* timeUniform = nullptr;
			Shader::Uniform* deltaTimeUniform = nullptr;

			Shader::Uniform* vegetationUniform = nullptr;
			Shader::Uniform* invertUVsUniform = nullptr;
			Shader::Uniform* staticMeshUniform = nullptr;
			Shader::Uniform* twoSidedUniform = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;

			Shader::Uniform* jitterCurrent = nullptr;
			Shader::Uniform* jitterLast = nullptr;

			static Shader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};


	}

}

#endif