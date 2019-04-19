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

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Window* window, Font* font, std::string text, float x, float y, vec4 color,
					float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void Render(Window* window, Font* font, std::string text, float x, float y, vec4 color, vec4 clipArea,
					vec4 blendArea, float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void RenderOutlined(Window* window, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor,
					float outlineScale, float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void RenderOutlined(Window* window, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
					vec4 clipArea, vec4 blendArea, float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void Update();

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			std::vector<vec3> CalculateCharacterInstances(Font* font, std::string text, int32_t* characterCount);

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* glyphsTexture;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* characterScales;
			Shader::Uniform* characterSizes;
			Shader::Uniform* textOffset;
			Shader::Uniform* textScale;
			Shader::Uniform* textColor;
			Shader::Uniform* outlineColor;
			Shader::Uniform* outlineScale;
			Shader::Uniform* edgeValue;
			Shader::Uniform* smoothness;
			Shader::Uniform *clipArea;
			Shader::Uniform *blendArea;

		};


	}

}

#endif