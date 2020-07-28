#ifndef AE_TERRAINRENDERER_H
#define AE_TERRAINRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../shader/ShaderBatch.h"

namespace Atlas {

	namespace Renderer {

		class TerrainRenderer : public Renderer {

		public:
			TerrainRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
				Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap);

		private:
			void GetUniforms();

			struct TerrainMaterial {
				uint32_t idx;
				
				float roughness;
				float metalness;
				float ao;
				float displacementScale;
				float normalScale;

				float padding0;
				float padding1;
			};

			Shader::ShaderBatch shaderBatch;

			Shader::ShaderConfig detailConfig;
			Shader::ShaderConfig distanceConfig;

			Buffer::Buffer terrainMaterialBuffer;

			Shader::Uniform* heightScale = nullptr;
			Shader::Uniform* offset = nullptr;
			Shader::Uniform* tileScale = nullptr;
			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* nodeSideLength = nullptr;
			Shader::Uniform* nodeLocation = nullptr;

			Shader::Uniform* leftLoD = nullptr;
			Shader::Uniform* topLoD = nullptr;
			Shader::Uniform* rightLoD = nullptr;
			Shader::Uniform* bottomLoD = nullptr;

			Shader::Uniform* patchSize = nullptr;

			Shader::Uniform* tessellationFactor = nullptr;
			Shader::Uniform* tessellationSlope = nullptr;
			Shader::Uniform* tessellationShift = nullptr;
			Shader::Uniform* maxTessellationLevel = nullptr;

			Shader::Uniform* displacementDistance = nullptr;

			Shader::Uniform* cameraLocation = nullptr;
			Shader::Uniform* frustumPlanes = nullptr;

			Shader::Uniform* normalTexelSize = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;

		};

	}

}

#endif
