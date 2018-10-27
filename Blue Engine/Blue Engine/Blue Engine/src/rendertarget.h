#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "system.h"
#include "framebuffer.h"

class RenderTarget {

public:
	RenderTarget();

	void Resize(int32_t width, int32_t height);

};


#endif