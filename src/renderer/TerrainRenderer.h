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

			void GetUniforms();

			static std::string vertexPath;
			static std::string tessControlPath;
			static std::string tessEvalPath;
			static std::string geometryPath;
			static std::string fragmentPath;

		private:
			Shader::Shader nearShader;
			Shader::Shader middleShader;
			Shader::Shader farShader;

			Shader::Uniform* heightField = nullptr;
			Shader::Uniform* normalMap = nullptr;
			Shader::Uniform* diffuseMap = nullptr;
			Shader::Uniform* splatMap = nullptr;

			Shader::Uniform* heightScale = nullptr;
			Shader::Uniform* offset = nullptr;
			Shader::Uniform* tileScale = nullptr;
			Shader::Uniform* modelMatrix = nullptr;
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

			struct MaterialUniform {
				Shader::Uniform* diffuseMap;
				Shader::Uniform* normalMap;
				Shader::Uniform* displacementMap;

				Shader::Uniform* diffuseColor;

				Shader::Uniform* specularHardness;
				Shader::Uniform* specularIntensity;

				Shader::Uniform* displacementScale;
			}materials[4];

		};

	}

}

#endif
