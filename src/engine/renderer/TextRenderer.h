#ifndef AE_TEXTRENDERER_H
#define AE_TEXTRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../Font.h"
#include "buffer/VertexArray.h"

namespace Atlas {

	namespace Renderer {

		class TextRenderer : public Renderer {

		public:
			TextRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

			void Render(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color = vec4(1.0f),
					float scale = 1.0f);

			void Render(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 clipArea,
					vec4 blendArea, float scale = 1.0f);

			void RenderOutlined(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor,
					float outlineScale, float scale = 1.0f);

			void RenderOutlined(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
					vec4 clipArea, vec4 blendArea, float scale = 1.0f);

			void Update();

		private:
			void GetUniforms();

			std::vector<vec3> CalculateCharacterInstances(Font* font, std::string text, int32_t* characterCount);

			OldBuffer::VertexArray vertexArray;

			OldShader::OldShader shader;

			OldShader::Uniform* projectionMatrix = nullptr;
			OldShader::Uniform* characterScales = nullptr;
			OldShader::Uniform* characterSizes = nullptr;
			OldShader::Uniform* textOffset = nullptr;
			OldShader::Uniform* textScale = nullptr;
			OldShader::Uniform* textColor = nullptr;
			OldShader::Uniform* outlineColor = nullptr;
			OldShader::Uniform* outlineScale = nullptr;
			OldShader::Uniform* edgeValue = nullptr;
			OldShader::Uniform* smoothness = nullptr;
			OldShader::Uniform* clipArea = nullptr;
			OldShader::Uniform* blendArea = nullptr;

		};


	}

}

#endif