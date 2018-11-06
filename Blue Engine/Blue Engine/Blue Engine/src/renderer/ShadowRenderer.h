#ifndef SHADOWRENDERER_H
#define SHADOWRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class ShadowRenderer : public IRenderer {

public:
	ShadowRenderer(const char* vertexSource, const char* fragmentSource);

	void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

};


#endif