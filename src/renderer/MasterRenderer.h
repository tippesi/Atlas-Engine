#ifndef AE_MASTERRENDERER_H
#define AE_MASTERRENDERER_H

#include "../System.h"
#include "buffer/VertexArray.h"

#include "GeometryRenderer.h"
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
	void RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

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
	void RenderTexture(Window* window, Texture2D* texture, float x, float y, float width, float height,
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
	void RenderTexture(Window* window, Texture2D* texture, float x, float y, float width, float height,
		vec4 clipArea, vec4 blendArea, bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

	void RenderTexture(Window* window, Texture2DArray* texture, int32_t depth, float x, float y,
		float width, float height, bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

	void RenderTexture(Window* window, Texture2DArray* texture, int32_t depth, float x, float y,
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
	void RenderRectangle(Window* window, vec4 color, float x, float y, float width, float height,
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
	void RenderRectangle(Window* window, vec4 color, float x, float y, float width, float height,
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

	VertexArray vertexArray;

	Shader rectangleShader;
	Shader texture2DShader;
	Shader texture2DArrayShader;

	Uniform* rectangleProjectionMatrix;
	Uniform* rectangleOffset;
	Uniform* rectangleScale;
	Uniform* rectangleColor;
	Uniform* rectangleClipArea;
	Uniform* rectangleBlendArea;

	Uniform* texture2DProjectionMatrix;
	Uniform* texture2DOffset;
	Uniform* texture2DScale;
	Uniform* texture2DTexture;
	Uniform* texture2DClipArea;
	Uniform* texture2DBlendArea;

	Uniform* texture2DArrayProjectionMatrix;
	Uniform* texture2DArrayOffset;
	Uniform* texture2DArrayScale;
	Uniform* texture2DArrayTexture;
	Uniform* texture2DArrayClipArea;
	Uniform* texture2DArrayBlendArea;
	Uniform* texture2DArrayDepth;

	GeometryRenderer geometryRenderer;
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

#endif