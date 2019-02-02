#ifndef AE_SHADOWRENDERER_H
#define AE_SHADOWRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "../shader/ShaderBatch.h"

#include <mutex>

class ShadowRenderer : public IRenderer {

public:
	ShadowRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static void InitShaderBatch();

	static void AddConfig(ShaderConfig* config);

	static void RemoveConfig(ShaderConfig* config);

	static std::string vertexPath;
	static std::string fragmentPath;

private:
	Framebuffer* framebuffer;

	Uniform* arrayMapUniform;
	Uniform* diffuseMapUniform;
	Uniform* lightSpaceMatrixUniform;
	Uniform* modelMatrixUniform;

	static ShaderBatch shaderBatch;
	static std::mutex shaderBatchMutex;

};


#endif