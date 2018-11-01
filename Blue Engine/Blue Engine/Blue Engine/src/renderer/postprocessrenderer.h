#ifndef POSTPROCESSRENDERER_H
#define POSTPROCESSRENDERER_H

#include "../system.h"
#include "IRenderer.h"
#include "../shader/shader.h"

class PostProcessRenderer : public IRenderer {

public:
	PostProcessRenderer(const char* vertexSource, const char* fragmentSource);

	virtual void Render(RenderTarget* target, Camera* camera, Scene* scene);

private:
	Shader* shader;

	Uniform* exposure;
	Uniform* saturation;

};

#endif