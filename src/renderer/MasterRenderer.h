#ifndef AE_MASTERRENDERER_H
#define AE_MASTERRENDERER_H

#include "../System.h"
#include "buffer/VertexArray.h"

#include "OpaqueRenderer.h"
#include "TerrainRenderer.h"
#include "ShadowRenderer.h"
#include "DecalRenderer.h"
#include "DirectionalVolumetricRenderer.h"
#include "DirectionalLightRenderer.h"
#include "PointLightRenderer.h"
#include "SkyboxRenderer.h"
#include "AtmosphereRenderer.h"
#include "PostProcessRenderer.h"
#include "TextRenderer.h"

namespace Atlas {

	namespace Renderer {

		class MasterRenderer {

		public:
			MasterRenderer();

			~MasterRenderer();

			/**
             *
             * @param window
             * @param target
             * @param camera
             * @param scene
             */
			void RenderScene(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			/**
             *
             * @param window
             * @param texture
             * @param x
             * @param y
             * @param width
             * @param height
             * @param alphaBlending
             * @param framebuffer
             */
			void RenderTexture(Viewport* viewport, Texture::Texture2D* texture, float x, float y, float width, float height,
							   bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

			/**
             *
             * @param window
             * @param texture
             * @param x
             * @param y
             * @param width
             * @param height
             * @param clipArea
             * @param blendArea
             * @param alphaBlending
             * @param framebuffer
             */
			void RenderTexture(Viewport* viewport, Texture::Texture2D* texture, float x, float y, float width, float height,
							   vec4 clipArea, vec4 blendArea, bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

			/**
			 *
			 * @param window
			 * @param texture
			 * @param depth
			 * @param x
			 * @param y
			 * @param width
			 * @param height
			 * @param alphaBlending
			 * @param framebuffer
			 */
			void RenderTexture(Viewport* viewport, Texture::Texture2DArray* texture, int32_t depth, float x, float y,
							   float width, float height, bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

			/**
			 *
			 * @param window
			 * @param texture
			 * @param depth
			 * @param x
			 * @param y
			 * @param width
			 * @param height
			 * @param clipArea
			 * @param blendArea
			 * @param alphaBlending
			 * @param framebuffer
			 */
			void RenderTexture(Viewport* viewport, Texture::Texture2DArray* texture, int32_t depth, float x, float y,
							   float width, float height, vec4 clipArea, vec4 blendArea, bool alphaBlending = false,
							   Framebuffer* framebuffer = nullptr);

			/**
             *
             * @param window
             * @param color
             * @param x
             * @param y
             * @param width
             * @param height
             * @param alphaBlending
             * @param framebuffer
             */
			void RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
								 bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

			/**
             *
             * @param window
             * @param color
             * @param x
             * @param y
             * @param width
             * @param height
             * @param clipArea
             * @param blendArea
             * @param alphaBlending
             * @param framebuffer
             */
			void RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
								 vec4 clipArea, vec4 blendArea, bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

			/**
             * Update of the renderer
             * @warning Must be called every frame
             */
			void Update();

			TextRenderer textRenderer;

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader rectangleShader;
			Shader::Shader texture2DShader;
			Shader::Shader texture2DArrayShader;

			Shader::Uniform* rectangleProjectionMatrix;
			Shader::Uniform* rectangleOffset;
			Shader::Uniform* rectangleScale;
			Shader::Uniform* rectangleColor;
			Shader::Uniform* rectangleClipArea;
			Shader::Uniform* rectangleBlendArea;

			Shader::Uniform* texture2DProjectionMatrix;
			Shader::Uniform* texture2DOffset;
			Shader::Uniform* texture2DScale;
			Shader::Uniform* texture2DTexture;
			Shader::Uniform* texture2DClipArea;
			Shader::Uniform* texture2DBlendArea;

			Shader::Uniform* texture2DArrayProjectionMatrix;
			Shader::Uniform* texture2DArrayOffset;
			Shader::Uniform* texture2DArrayScale;
			Shader::Uniform* texture2DArrayTexture;
			Shader::Uniform* texture2DArrayClipArea;
			Shader::Uniform* texture2DArrayBlendArea;
			Shader::Uniform* texture2DArrayDepth;

			OpaqueRenderer opaqueRenderer;
			TerrainRenderer terrainRenderer;
			ShadowRenderer shadowRenderer;
			DecalRenderer decalRenderer;
			DirectionalVolumetricRenderer directionalVolumetricRenderer;
			DirectionalLightRenderer directionalLightRenderer;
			PointLightRenderer pointLightRenderer;
			SkyboxRenderer skyboxRenderer;
			AtmosphereRenderer atmosphereRenderer;
			PostProcessRenderer postProcessRenderer;

		};

	}

}

#endif