#ifndef AE_MASTERRENDERER_H
#define AE_MASTERRENDERER_H

#include "../System.h"
#include "buffer/VertexArray.h"

#include "OpaqueRenderer.h"
#include "TerrainRenderer.h"
#include "OceanRenderer.h"
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
			OceanRenderer oceanRenderer;
			AtmosphereRenderer atmosphereRenderer;

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Shader::Shader rectangleShader;
			Shader::Shader texture2DShader;
			Shader::Shader texture2DArrayShader;

			Shader::Uniform* rectangleProjectionMatrix = nullptr;
			Shader::Uniform* rectangleOffset = nullptr;
			Shader::Uniform* rectangleScale = nullptr;
			Shader::Uniform* rectangleColor = nullptr;
			Shader::Uniform* rectangleClipArea = nullptr;
			Shader::Uniform* rectangleBlendArea = nullptr;

			Shader::Uniform* texture2DProjectionMatrix = nullptr;
			Shader::Uniform* texture2DOffset = nullptr;
			Shader::Uniform* texture2DScale = nullptr;
			Shader::Uniform* texture2DClipArea = nullptr;
			Shader::Uniform* texture2DBlendArea = nullptr;

			Shader::Uniform* texture2DArrayProjectionMatrix = nullptr;
			Shader::Uniform* texture2DArrayOffset = nullptr;
			Shader::Uniform* texture2DArrayScale = nullptr;
			Shader::Uniform* texture2DArrayClipArea = nullptr;
			Shader::Uniform* texture2DArrayBlendArea = nullptr;
			Shader::Uniform* texture2DArrayDepth = nullptr;

			OpaqueRenderer opaqueRenderer;
			TerrainRenderer terrainRenderer;
			ShadowRenderer shadowRenderer;
			DecalRenderer decalRenderer;
			DirectionalVolumetricRenderer directionalVolumetricRenderer;
			DirectionalLightRenderer directionalLightRenderer;
			PointLightRenderer pointLightRenderer;
			SkyboxRenderer skyboxRenderer;
			PostProcessRenderer postProcessRenderer;

		};

	}

}

#endif