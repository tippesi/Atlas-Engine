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

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color = vec4(1.0f),
					float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void Render(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 clipArea,
					vec4 blendArea, float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void RenderOutlined(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor,
					float outlineScale, float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void RenderOutlined(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
					vec4 clipArea, vec4 blendArea, float scale = 1.0f, Framebuffer* framebuffer = nullptr);

			void Update();

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			std::vector<vec3> CalculateCharacterInstances(Font* font, std::string text, int32_t* characterCount);

			Buffer::VertexArray vertexArray;

			Shader::Shader shader;

			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* characterScales = nullptr;
			Shader::Uniform* characterSizes = nullptr;
			Shader::Uniform* textOffset = nullptr;
			Shader::Uniform* textScale = nullptr;
			Shader::Uniform* textColor = nullptr;
			Shader::Uniform* outlineColor = nullptr;
			Shader::Uniform* outlineScale = nullptr;
			Shader::Uniform* edgeValue = nullptr;
			Shader::Uniform* smoothness = nullptr;
			Shader::Uniform* clipArea = nullptr;
			Shader::Uniform* blendArea = nullptr;

		};


	}

}

#endif