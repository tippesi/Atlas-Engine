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

			void RenderImpostor(Viewport* viewport, std::vector<mat4> viewMatrices,
				mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

			static void InitShaderBatch();

			static void AddConfig(OldShader::ShaderConfig* config);

			static void RemoveConfig(OldShader::ShaderConfig* config);

		private:
			void AdjustFaceCulling(bool cullFaces, bool& state);

			RenderList renderList;

			ImpostorRenderer impostorRenderer;

			OldShader::Uniform* modelMatrixUniform = nullptr;
			OldShader::Uniform* viewMatrixUniform = nullptr;
			OldShader::Uniform* projectionMatrixUniform = nullptr;

			OldShader::Uniform* cameraLocationUniform = nullptr;
			OldShader::Uniform* baseColorUniform = nullptr;
			OldShader::Uniform* roughnessUniform = nullptr;
			OldShader::Uniform* metalnessUniform = nullptr;
			OldShader::Uniform* aoUniform = nullptr;
			OldShader::Uniform* normalScaleUniform = nullptr;
			OldShader::Uniform* displacementScaleUniform = nullptr;
			OldShader::Uniform* materialIdxUniform = nullptr;

			OldShader::Uniform* timeUniform = nullptr;
			OldShader::Uniform* deltaTimeUniform = nullptr;

			OldShader::Uniform* vegetationUniform = nullptr;
			OldShader::Uniform* invertUVsUniform = nullptr;
			OldShader::Uniform* staticMeshUniform = nullptr;
			OldShader::Uniform* twoSidedUniform = nullptr;

			OldShader::Uniform* pvMatrixLast = nullptr;

			OldShader::Uniform* jitterCurrent = nullptr;
			OldShader::Uniform* jitterLast = nullptr;

			static OldShader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};


	}

}

#endif