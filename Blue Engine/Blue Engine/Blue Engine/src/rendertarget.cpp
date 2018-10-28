#include "rendertarget.h"

RenderTarget::RenderTarget(int32_t width, int32_t height) {

	framebuffer = new Framebuffer(width, height);

	framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, GL_FLOAT, GL_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	framebuffer->AddComponent(GL_DEPTH_ATTACHMENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT24, GL_CLAMP_TO_EDGE, GL_LINEAR);

}

void RenderTarget::Resize(int32_t width, int32_t height) {

	framebuffer->Resize(width, height);

}