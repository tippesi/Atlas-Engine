#ifndef MASTERRENDERER_H
#define MASTERRENDERER_H

#include "../System.h"
#include "../VertexArray.h"

#include "GeometryRenderer.h"
#include "TerrainRenderer.h"
#include "DirectionalShadowRenderer.h"
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

	void RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	void RenderTexture(Window* window, Texture* texture, int32_t x, int32_t y, int32_t width, int32_t height, 
		bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

	void RenderRectangle(Window* window, vec4 color, int32_t x, int32_t y, int32_t width, int32_t height,
		bool alphaBlending = false, Framebuffer* framebuffer = nullptr);

	~MasterRenderer();

	TextRenderer* textRenderer;

private:
	void GetUniforms();

	VertexArray * vertexArray;

	Shader* rectangleShader;
	Shader* texturedRectangleShader;

	Uniform* rectangleProjectionMatrix;
	Uniform* rectangleOffset;
	Uniform* rectangleScale;
	Uniform* rectangleColor;

	Uniform* texturedRectangleProjectionMatrix;
	Uniform* texturedRectangleOffset;
	Uniform* texturedRectangleScale;
	Uniform* texturedRectangleTexture;

	GeometryRenderer* geometryRenderer;
	TerrainRenderer* terrainRenderer;
	DirectionalShadowRenderer* directionalShadowRenderer;
	DirectionalVolumetricRenderer* directionalVolumetricRenderer;
	DirectionalLightRenderer* directionalLightRenderer;
	PointLightRenderer* pointLightRenderer;
	SkyboxRenderer* skyboxRenderer;
	AtmosphereRenderer* atmosphereRenderer;
	PostProcessRenderer* postProcessRenderer;

	static string terrainVertexPath;
	static string terrainTessControlPath;
	static string terrainTessEvalPath;
	static string terrainGeometryPath;
	static string terrainFragmentPath;
	static string shadowVertexPath;
	static string shadowFragmentPath;
	static string volumetricVertexPath;
	static string volumetricFragmentPath;
	static string bilateralBlurVertexPath;
	static string bilateralBlurFragmentPath;
	static string directionalLightVertexPath;
	static string directionalLightFragmentPath;
	static string pointLightVertexPath;
	static string pointLightFragmentPath;
	static string skyboxVertexPath;
	static string skyboxFragmentPath;
	static string atmosphereVertexPath;
	static string atmosphereFragmentPath;
	static string postProcessVertexPath;
	static string postProcessFragmentPath;
	static string textVertexPath;
	static string textFragmentPath;
	static string rectangleVertexPath;
	static string rectangleFragmentPath;

};

#endif