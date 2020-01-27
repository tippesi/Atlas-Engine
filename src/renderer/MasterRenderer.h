#ifndef AE_MASTERRENDERER_H
#define AE_MASTERRENDERER_H

#include "../System.h"
#include "buffer/VertexArray.h"

#include "RenderBatch.h"

#include "OpaqueRenderer.h"
#include "TerrainRenderer.h"
#include "OceanRenderer.h"
#include "ShadowRenderer.h"
#include "TerrainShadowRenderer.h"
#include "DecalRenderer.h"
#include "DirectionalVolumetricRenderer.h"
#include "DirectionalLightRenderer.h"
#include "PointLightRenderer.h"
#include "TemporalAARenderer.h"
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
			void RenderScene(Viewport* viewport, RenderTarget* target, Camera* camera,
				Scene::Scene* scene, Texture::Texture2D* texture = nullptr, 
				RenderBatch* batch = nullptr);

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


			void RenderBatched(Viewport* viewport, Camera* camera, RenderBatch* batch);

			/**
             * Update of the renderer
             * @warning Must be called every frame
             */
			void Update();

			TextRenderer textRenderer;
			OceanRenderer oceanRenderer;
			AtmosphereRenderer atmosphereRenderer;

		private:
			void GetUniforms();

			Framebuffer framebuffer;
			Framebuffer depthFramebuffer;

			Buffer::VertexArray vertexArray;

			Shader::Shader rectangleShader;
			Shader::Shader texture2DShader;
			Shader::Shader texture2DArrayShader;

			Shader::Shader lineShader;

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

			Shader::Uniform* lineViewMatrix = nullptr;
			Shader::Uniform* lineProjectionMatrix = nullptr;

			OpaqueRenderer opaqueRenderer;
			TerrainRenderer terrainRenderer;
			ShadowRenderer shadowRenderer;
			TerrainShadowRenderer terrainShadowRenderer;
			DecalRenderer decalRenderer;
			DirectionalVolumetricRenderer directionalVolumetricRenderer;
			DirectionalLightRenderer directionalLightRenderer;
			PointLightRenderer pointLightRenderer;
			TemporalAARenderer taaRenderer;
			SkyboxRenderer skyboxRenderer;
			PostProcessRenderer postProcessRenderer;

			std::vector<vec2> haltonSequence;
			size_t haltonIndex = 0;

		};

	}

}

#endif