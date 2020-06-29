#ifndef AE_TERRAINRENDERER_H
#define AE_TERRAINRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TerrainRenderer : public Renderer {

		public:
			TerrainRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {}

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
				Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap);

		private:
			void GetUniforms();

			void GetDistanceUniforms();

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

			Shader::Shader shader;
			Shader::Shader distanceShader;

			Buffer::Buffer terrainMaterialBuffer;

			// Near shader uniforms
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

			// Distance shader uniforms
			Shader::Uniform* distanceHeightScale = nullptr;
			Shader::Uniform* distanceTileScale = nullptr;
			Shader::Uniform* distanceViewMatrix = nullptr;
			Shader::Uniform* distanceProjectionMatrix = nullptr;
			Shader::Uniform* distanceNodeSideLength = nullptr;
			Shader::Uniform* distanceNodeLocation = nullptr;

			Shader::Uniform* distanceLeftLoD = nullptr;
			Shader::Uniform* distanceTopLoD = nullptr;
			Shader::Uniform* distanceRightLoD = nullptr;
			Shader::Uniform* distanceBottomLoD = nullptr;

			Shader::Uniform* distancePatchSize = nullptr;

			Shader::Uniform* distanceCameraLocation = nullptr;
			Shader::Uniform* distanceFrustumPlanes = nullptr;

			Shader::Uniform* distanceNormalTexelSize = nullptr;

			Shader::Uniform* distancePvMatrixLast = nullptr;
			Shader::Uniform* distanceJitterLast = nullptr;
			Shader::Uniform* distanceJitterCurrent = nullptr;

		};

	}

}

#endif
