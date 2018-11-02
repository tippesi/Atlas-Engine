#include "rendertarget.h"

RenderTarget::RenderTarget(int32_t width, int32_t height) : width(width), height(height) {

	geometryFramebuffer = new Framebuffer(width, height);

	geometryFramebuffer->AddComponent(GL_COLOR_ATTACHMENT0, GL_UNSIGNED_BYTE, GL_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
	geometryFramebuffer->AddComponent(GL_COLOR_ATTACHMENT1, GL_UNSIGNED_BYTE, GL_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
	geometryFramebuffer->AddComponent(GL_COLOR_ATTACHMENT2, GL_FLOAT, GL_RG16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	geometryFramebuffer->AddComponent(GL_DEPTH_ATTACHMENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT24, GL_CLAMP_TO_EDGE, GL_LINEAR);

	postProcessingFramebuffer = new Framebuffer(width, height);

	postProcessingFramebuffer->AddComponent(GL_COLOR_ATTACHMENT0, GL_FLOAT, GL_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	postProcessingFramebuffer->AddComponent(GL_COLOR_ATTACHMENT1, GL_FLOAT, GL_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

}

void RenderTarget::Resize(int32_t width, int32_t height) {

	geometryFramebuffer->Resize(width, height);
	postProcessingFramebuffer->Resize(width, height);

	this->width = width;
	this->height = height;

}

RenderTarget::~RenderTarget() {

	delete geometryFramebuffer;
	delete postProcessingFramebuffer;

}