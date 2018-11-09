#ifndef POSTPROCESSRENDERER_H
#define POSTPROCESSRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "../shader/Shader.h"

class PostProcessRenderer : public IRenderer {

public:
	PostProcessRenderer(const char* vertexSource, const char* fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

private:
	void GetUniforms(bool deleteUniforms);

	uint32_t rectangleVAO;

	Shader* shader;

	Uniform* hdrTexture;
	Uniform* bloomFirstTexture;
	Uniform* bloomSecondTexture;
	Uniform* bloomThirdTexture;
	Uniform* exposure;
	Uniform* saturation;
	Uniform* bloomPassses;
	Uniform* aberrationStrength;
	Uniform* aberrationReversed;
	Uniform* vignetteOffset;
	Uniform* vignettePower;
	Uniform* vignetteStrength;
	Uniform* vignetteColor;

};

#endif