#ifndef IRENDERER_H
#define IRENDERER_H

#include "../system.h"
#include "../rendertarget.h"
#include "../camera.h"
#include "../scene.h"


class IRenderer {

public:
	virtual void Render(RenderTarget* target, Camera* camera, Scene* scene) = 0;
	virtual ~IRenderer() {}

};

#endif