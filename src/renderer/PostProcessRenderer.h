#ifndef POSTPROCESSRENDERER_H
#define POSTPROCESSRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "../shader/Shader.h"

class PostProcessRenderer : public IRenderer {

public:
	PostProcessRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static string vertexPath;
	static string fragmentPath;

private:
	void GetUniforms();

	Shader* shader;

	Uniform* hdrTexture;
	Uniform* bloomFirstTexture;
	Uniform* bloomSecondTexture;
	Uniform* bloomThirdTexture;
	Uniform* hdrTextureResolution;
	Uniform* exposure;
	Uniform* saturation;
	Uniform* bloomPassses;
	Uniform* aberrationStrength;
	Uniform* aberrationReversed;
	Uniform* vignetteOffset;
	Uniform* vignettePower;
	Uniform* vignetteStrength;
	Uniform* vignetteColor;
	Uniform* timeInMilliseconds;

};

#endif