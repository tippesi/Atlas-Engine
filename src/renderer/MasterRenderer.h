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
#include "DirectionalLightRenderer.h"
#include "PointLightRenderer.h"
#include "IndirectLightRenderer.h"
#include "TemporalAARenderer.h"
#include "SkyboxRenderer.h"
#include "AtmosphereRenderer.h"
#include "PostProcessRenderer.h"
#include "GBufferDownscaleRenderer.h"
#include "TextRenderer.h"
#include "DDGIRenderer.h"
#include "SSAORenderer.h"
#include "RTAORenderer.h"
#include "VolumetricRenderer.h"
#include "VegetationRenderer.h"

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

			/**
			 *
			 * @param viewport
			 * @param camera
			 * @param batch
			 */
			void RenderBatched(Viewport* viewport, Camera* camera, RenderBatch* batch);

            /**
             * Renders the scene into an environment probe
             * @param probe The environment probe.
             * @param target A render target to support rendering
             * @param scene The scene which should be rendered-
             * @note The render target must have the same resolution as the probe.
             */
			void RenderProbe(Lighting::EnvironmentProbe* probe, RenderTarget* target, Scene::Scene* scene);

			/**
			 * Filters an environment probe.
			 * @param probe The environment probe.
			 * @note A probe has to be filtered to support image based lighting
			 */
			void FilterProbe(Lighting::EnvironmentProbe* probe);

			/**
             * Update of the renderer
             * @warning Must be called every frame
             */
			void Update();

			TextRenderer textRenderer;
			OceanRenderer oceanRenderer;
			AtmosphereRenderer atmosphereRenderer;

		private:
			struct PackedMaterial {

				int32_t baseColor;
				int32_t emissiveColor;
				int32_t transmissionColor;

				float emissiveIntensity;

				int32_t data0;
				int32_t data1;

				int32_t features;
				int32_t unused;

			};

			void GetUniforms();

			void PrepareMaterials(Scene::Scene* scene, std::vector<PackedMaterial>& materials,
				std::unordered_map<void*, uint16_t>& materialMap);

			void PreintegrateBRDF();

			Framebuffer framebuffer;
			Framebuffer depthFramebuffer;

			Texture::Texture2D dfgPreintegrationTexture;

			Buffer::VertexArray vertexArray;
			Buffer::VertexArray cubeVertexArray;

			Shader::Shader rectangleShader;
			Shader::Shader texture2DShader;
			Shader::Shader texture2DArrayShader;

			Shader::Shader lineShader;

			Shader::Shader createProbeFaceShader;
			Shader::Shader filterDiffuseShader;
			Shader::Shader filterSpecularShader;

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
			VegetationRenderer vegetationRenderer;
			TerrainShadowRenderer terrainShadowRenderer;
			DecalRenderer decalRenderer;
			DirectionalLightRenderer directionalLightRenderer;
			IndirectLightRenderer indirectLightRenderer;

			TemporalAARenderer taaRenderer;
			SkyboxRenderer skyboxRenderer;
			PostProcessRenderer postProcessRenderer;
			GBufferDownscaleRenderer downscaleRenderer;
			DDGIRenderer ddgiRenderer;
			SSAORenderer ssaoRenderer;
			RTAORenderer rtaoRenderer;
			VolumetricRenderer volumetricRenderer;

			std::vector<vec2> haltonSequence;
			size_t haltonIndex = 0;

		};

	}

}

#endif