#ifndef SHADOWRENDERER_H
#define SHADOWRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "../shader/ShaderBatch.h"

class DirectionalShadowRenderer : public IRenderer {

public:
	DirectionalShadowRenderer(string vertexSource, string fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	static void InitShaderBatch(string vertexSource, string fragmentSource);

	static void AddConfig(ShaderConfig* config);

	static void RemoveConfig(ShaderConfig* config);

private:
	static ShaderBatch* shaderBatch;

	Framebuffer* framebuffer;

	Uniform* diffuseMapUniform;
	Uniform* lightSpaceMatrixUniform;
	Uniform* modelMatrixUniform;

};


#endif