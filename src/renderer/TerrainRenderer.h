#ifndef AE_TERRAINRENDERER_H
#define AE_TERRAINRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TerrainRenderer : public Renderer {

		public:
			TerrainRenderer();

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene);

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

			Shader::Uniform* heightField;
			Shader::Uniform* normalMap;
			Shader::Uniform* diffuseMap;
			Shader::Uniform* splatMap;

			Shader::Uniform* heightScale;
			Shader::Uniform* offset;
			Shader::Uniform* tileScale;
			Shader::Uniform* modelMatrix;
			Shader::Uniform* viewMatrix;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* cameraLocation;
			Shader::Uniform* nodeSideLength;
			Shader::Uniform* nodeLocation;

			Shader::Uniform* patchOffsetsScale;

			Shader::Uniform* tessellationFactor;
			Shader::Uniform* tessellationSlope;
			Shader::Uniform* tessellationShift;
			Shader::Uniform* maxTessellationLevel;

			Shader::Uniform* displacementDistance;

			Shader::Uniform* frustumPlanes;

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
