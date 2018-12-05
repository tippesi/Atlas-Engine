#ifndef SHADOWRENDERER_H
#define SHADOWRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "../shader/ShaderBatch.h"

#include <mutex>

class ShadowRenderer : public IRenderer {

public:
	ShadowRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	static void InitShaderBatch();

	static void AddConfig(ShaderConfig* config);

	static void RemoveConfig(ShaderConfig* config);

	static string vertexPath;
	static string fragmentPath;

private:
	Framebuffer* framebuffer;

	Uniform* diffuseMapUniform;
	Uniform* lightSpaceMatrixUniform;
	Uniform* modelMatrixUniform;

	static ShaderBatch* shaderBatch;
	static mutex shaderBatchMutex;

};


#endif