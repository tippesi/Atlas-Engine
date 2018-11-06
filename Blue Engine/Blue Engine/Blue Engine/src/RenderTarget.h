#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "system.h"
#include "framebuffer.h"

class RenderTarget {

public:
	RenderTarget(int32_t width, int32_t height);

	void Resize(int32_t width, int32_t height);

	~RenderTarget();

	Framebuffer* geometryFramebuffer;
	Framebuffer* postProcessingFramebuffer;

	int32_t width;
	int32_t height;

};


#endif