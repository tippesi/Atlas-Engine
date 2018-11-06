#ifndef GEOMETRYRENDERER_H
#define GEOMETRYRENDERER_H

#include "../system.h"
#include "IRenderer.h"

class GeometryRenderer : public IRenderer {

public:
	GeometryRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

};

#endif