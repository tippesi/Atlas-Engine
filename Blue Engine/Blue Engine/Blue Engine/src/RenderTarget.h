#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "System.h"
#include "Framebuffer.h"

class RenderTarget {

public:
	RenderTarget(int32_t width, int32_t height);

	void Resize(int32_t width, int32_t height);

	~RenderTarget();

	Framebuffer* geometryFramebuffer;
	Framebuffer* lightingFramebuffer;

	int32_t width;
	int32_t height;

};


#endif