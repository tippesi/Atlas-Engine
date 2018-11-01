#ifndef MASTERRENDERER_H
#define MASTERRENDERER_H

#include "../system.h"

#include "postprocessrenderer.h"

class MasterRenderer {

public:
	MasterRenderer();

	void RenderScene();

	void RenderTexture();

	void RenderRectangle();


};

#endif