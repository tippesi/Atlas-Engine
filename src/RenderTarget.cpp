#include "RenderTarget.h"

RenderTarget::RenderTarget(int32_t width, int32_t height) : width(width), height(height) {

	// We want a shared depth texture across the geometry and lighting framebuffers
	Texture* depthTexture = new Texture(GL_UNSIGNED_INT, width, height, GL_DEPTH_COMPONENT24, 0.0f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

	geometryFramebuffer = new Framebuffer(width, height);

	geometryFramebuffer->AddComponent(GL_COLOR_ATTACHMENT0, GL_UNSIGNED_BYTE, GL_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
	geometryFramebuffer->AddComponent(GL_COLOR_ATTACHMENT1, GL_UNSIGNED_BYTE, GL_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
	geometryFramebuffer->AddComponent(GL_COLOR_ATTACHMENT2, GL_FLOAT, GL_RG16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	geometryFramebuffer->AddComponentTexture(GL_DEPTH_ATTACHMENT, depthTexture);

	lightingFramebuffer = new Framebuffer(width, height);

	lightingFramebuffer->AddComponent(GL_COLOR_ATTACHMENT0, GL_FLOAT, GL_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	lightingFramebuffer->AddComponentTexture(GL_DEPTH_ATTACHMENT, depthTexture);

}

void RenderTarget::Resize(int32_t width, int32_t height) {

	geometryFramebuffer->Resize(width, height);
	lightingFramebuffer->Resize(width, height);

	this->width = width;
	this->height = height;

}

RenderTarget::~RenderTarget() {

	geometryFramebuffer->DeleteContent();
	lightingFramebuffer->DeleteContent();

	delete geometryFramebuffer;
	delete lightingFramebuffer;

}