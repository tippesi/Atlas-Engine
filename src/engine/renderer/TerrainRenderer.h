#ifndef AE_TERRAINRENDERER_H
#define AE_TERRAINRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TerrainRenderer : public Renderer {

		public:
			TerrainRenderer();

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
				float tiling;

				float padding1;
			};

            /*
			OldShader::ShaderBatch shaderBatch;

			OldShader::ShaderConfig detailConfig;
			OldShader::ShaderConfig distanceConfig;

			Buffer::Buffer terrainMaterialBuffer;

			OldShader::Uniform* heightScale = nullptr;
			OldShader::Uniform* offset = nullptr;
			OldShader::Uniform* tileScale = nullptr;
			OldShader::Uniform* viewMatrix = nullptr;
			OldShader::Uniform* projectionMatrix = nullptr;
			OldShader::Uniform* nodeSideLength = nullptr;
			OldShader::Uniform* nodeLocation = nullptr;

			OldShader::Uniform* leftLoD = nullptr;
			OldShader::Uniform* topLoD = nullptr;
			OldShader::Uniform* rightLoD = nullptr;
			OldShader::Uniform* bottomLoD = nullptr;

			OldShader::Uniform* patchSize = nullptr;

			OldShader::Uniform* tessellationFactor = nullptr;
			OldShader::Uniform* tessellationSlope = nullptr;
			OldShader::Uniform* tessellationShift = nullptr;
			OldShader::Uniform* maxTessellationLevel = nullptr;

			OldShader::Uniform* displacementDistance = nullptr;

			OldShader::Uniform* cameraLocation = nullptr;
			OldShader::Uniform* frustumPlanes = nullptr;

			OldShader::Uniform* normalTexelSize = nullptr;

			OldShader::Uniform* pvMatrixLast = nullptr;
			OldShader::Uniform* jitterLast = nullptr;
			OldShader::Uniform* jitterCurrent = nullptr;
            */

		};

	}

}

#endif
