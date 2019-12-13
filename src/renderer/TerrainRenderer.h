#ifndef AE_TERRAINRENDERER_H
#define AE_TERRAINRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TerrainRenderer : public Renderer {

		public:
			TerrainRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string tessControlPath;
			static std::string tessEvalPath;
			static std::string fragmentPath;

			static std::string distanceVertexPath;
			static std::string distanceFragmentPath;

		private:
			void GetUniforms();

			void GetDistanceUniforms();

			Shader::Shader shader;
			Shader::Shader distanceShader;

			Shader::Uniform* heightField = nullptr;
			Shader::Uniform* normalMap = nullptr;
			Shader::Uniform* diffuseMap = nullptr;
			Shader::Uniform* splatMap = nullptr;

			Shader::Uniform* heightScale = nullptr;
			Shader::Uniform* offset = nullptr;
			Shader::Uniform* tileScale = nullptr;
			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;
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

			Shader::Uniform* frustumPlanes = nullptr;

			Shader::Uniform* normalTexelSize = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;

			struct MaterialUniform {
				Shader::Uniform* diffuseMap;
				Shader::Uniform* normalMap;
				Shader::Uniform* displacementMap;

				Shader::Uniform* diffuseColor;

				Shader::Uniform* specularHardness;
				Shader::Uniform* specularIntensity;

				Shader::Uniform* displacementScale;
			}materials[4], distanceMaterials[4];

			Shader::Uniform* distanceHeightField = nullptr;
			Shader::Uniform* distanceNormalMap = nullptr;
			Shader::Uniform* distanceDiffuseMap = nullptr;
			Shader::Uniform* distanceSplatMap = nullptr;

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

			Shader::Uniform* distanceNormalTexelSize = nullptr;

			Shader::Uniform* distancePvMatrixLast = nullptr;
			Shader::Uniform* distanceJitterLast = nullptr;
			Shader::Uniform* distanceJitterCurrent = nullptr;

			mat4 pvMatrixPrev;
			vec2 jitterPrev;

		};

	}

}

#endif
